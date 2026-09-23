/* Host self-test for src/boot/pef_loader.c.
 * Build: gcc -O2 -I../src/boot pef_loader_test.c ../src/boot/pef_loader.c -o pef_test
 * Run:   pef_test <system_data_fork.bin>
 */
#include "pef_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *path, size_t *outLen)
{
    FILE *fp = fopen(path, "rb");
    uint8_t *buf;
    long n;
    if (!fp) { perror(path); return NULL; }
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (uint8_t *)malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, fp) != (size_t)n) { fclose(fp); free(buf); return NULL; }
    fclose(fp);
    *outLen = (size_t)n;
    return buf;
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* Host allocator wrappers: the loader itself is freestanding (no libc). */
static void *wrap_alloc(void *ctx, size_t n) { (void)ctx; return calloc(1, n); }
static void wrap_free(void *ctx, void *p) { (void)ctx; free(p); }

#define MAXFRAG 4096
#define MAXFILES 8

int main(int argc, char **argv)
{
    const char *paths[MAXFILES];
    PefFragment *frags;
    uint8_t *files[MAXFILES];
    size_t lens[MAXFILES];
    int nFiles = 0;
    int n, i, a;

    if (argc > 1) {
        for (a = 1; a < argc && nFiles < MAXFILES; a++)
            paths[nFiles++] = argv[a];
    } else {
        paths[nFiles++] = "system_data_fork.bin";
    }

    frags = (PefFragment *)calloc(MAXFRAG, sizeof(*frags));
    for (a = 0; a < MAXFRAG; a++) {
        frags[a].alloc = wrap_alloc;
        frags[a].free_ = wrap_free;
    }
    for (a = 0; a < nFiles; a++) {
        files[a] = read_file(paths[a], &lens[a]);
        if (!files[a]) return 1;
    }
    n = 0;
    for (a = 0; a < nFiles && n < MAXFRAG; a++) {
        int got = PefEnumFragments(files[a], lens[a], frags + n, MAXFRAG - n);
        printf("file %s: %d fragments\n", paths[a], got);
        n += got;
    }
    printf("fragments: %d\n", n);

    for (i = 0; i < n; i++) {
        if (PefExpandSections(&frags[i]) != 0)
            printf("  frag[%d] expand FAILED\n", i);
    }

    printf("frag[0]: sections=%u inst=%u libs=%u syms=%u exports=%u relocSecs=%u\n",
           frags[0].sectionCount, frags[0].instSectionCount, frags[0].libCount,
           frags[0].symCount, frags[0].exportCount, frags[0].relocSectionCount);

    {
        uint32_t codeBase = 0x01000000u, dataBase = 0x02000000u;
        uint32_t toc = PefPlaceFragment(&frags[0], codeBase, dataBase);
        uint32_t *imp = (uint32_t *)calloc(frags[0].symCount, sizeof(uint32_t));
        PefFragment *f = &frags[0];

        for (i = 0; i < (int)f->symCount; i++)
            imp[i] = 0x00FF0000u + (uint32_t)i * 16u;

        printf("r2(toc) = 0x%08X\n", toc);
        if (f->initSection >= 0) {
            uint8_t *d = f->expanded[f->initSection];
            uint32_t e = be32(d + f->initOffset);
            uint32_t t = be32(d + f->initOffset + 4);
            printf("raw init tvec = {0x%08X, 0x%08X}\n", e, t);
        }

        /* Relocate every instantiated section exactly once. */
        for (i = 0; i < f->sectionCount; i++) {
            if (f->expanded[i] &&
                PefRelocateSection(f, i, f->expanded[i], imp) != 0)
                printf("  section %d relocate FAILED\n", i);
        }

        if (f->initSection >= 0) {
            uint8_t *d = f->expanded[f->initSection];
            uint32_t e = be32(d + f->initOffset);
            uint32_t t = be32(d + f->initOffset + 4);
            printf("reloc init tvec = {entry 0x%08X, toc 0x%08X} -> code off 0x%X\n",
                   e, t, e - codeBase);
        }

        {
            uint8_t *d = f->expanded[1];
            uint32_t slot = toc - dataBase - 0xB7Cu; /* data offset for stub -0xB7C */
            printf("stub(-0xB7C) data+0x%X = 0x%08X (expect import idx %u)\n",
                   slot, be32(d + slot), (be32(d + slot) - 0x00FF0000u) / 16u);
            for (i = 0; i < 3 && i < (int)f->exportCount; i++) {
                PefExportedSymbol *e = &f->exports[i];
                uint32_t v = e->symbolValue;
                printf("  export[%d] %-18s tvec@data+0x%X = {0x%08X, 0x%08X}\n",
                       i, e->name ? e->name : "?", v,
                       be32(d + v), be32(d + v + 4));
            }
        }
        free(imp);
    }

    /* Resolve every fragment's imports by symbol name across the whole set. */
    {
        long total = 0, resolved = 0;
        long libMiss[64];
        char libMissName[64][64];
        int nlibMiss = 0;
        int fi, k;
        for (fi = 0; fi < n; fi++) {
            PefFragment *f = &frags[fi];
            for (k = 0; k < (int)f->symCount; k++) {
                const char *nm = f->imports[k].name;
                total++;
                if (PefFindExport(frags, n, nm, NULL)) {
                    resolved++;
                } else {
                    const char *lib = "?";
                    int l, seen = 0;
                    for (l = 0; l < (int)f->libCount; l++) {
                        uint32_t idx = (uint32_t)k;
                        if (idx >= f->libs[l].firstImportedSymbol &&
                            idx < f->libs[l].firstImportedSymbol +
                                  f->libs[l].importedSymbolCount) {
                            lib = f->libs[l].name;
                            break;
                        }
                    }
                    for (l = 0; l < nlibMiss; l++) {
                        if (strcmp(libMissName[l], lib) == 0) {
                            libMiss[l]++;
                            seen = 1;
                            break;
                        }
                    }
                    if (!seen && nlibMiss < 64) {
                        snprintf(libMissName[nlibMiss], sizeof(libMissName[0]), "%s", lib);
                        libMiss[nlibMiss] = 1;
                        nlibMiss++;
                    }
                }
            }
        }
        printf("\nimports total=%ld resolved=%ld unresolved=%ld\n",
               total, resolved, total - resolved);
        printf("unresolved by library:\n");
        for (k = 0; k < nlibMiss; k++)
            printf("  %-32s %ld\n", libMissName[k], libMiss[k]);
    }

    for (i = 0; i < n; i++)
        PefFree(&frags[i]);
    free(frags);
    for (a = 0; a < nFiles; a++)
        free(files[a]);
    return 0;
}
