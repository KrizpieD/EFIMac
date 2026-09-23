/*
 * pef_loader.c - PEF / CFM fragment loader (see pef_loader.h).
 *
 * Bit layouts for the relocation bytecode and the pattern-initialized data
 * opcodes follow Apple's PEFBinaryFormat.h and the "Mac OS Runtime
 * Architectures" book.  The sequence of relocation variables (sectionC,
 * sectionD, importIndex) and the section-relative addressing match the
 * reference implementation; all defaultAddress fields observed in the Mac OS
 * System are zero, which this code relies on for the relocation offsets.
 */
#include "pef_loader.h"

/* Freestanding: the loader never calls libc, so it links in UEFI contexts
   (/nodefaultlib).  All memory comes from the PefFragment alloc callbacks and
   the few byte helpers below are provided locally. */

static uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t rd32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static int32_t rds32(const uint8_t *p) { return (int32_t)rd32(p); }
static int16_t rds16(const uint8_t *p) { return (int16_t)rd16(p); }

static void PelMove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s) return;
    if (d < s) { while (n--) *d++ = *s++; }
    else { d += n; s += n; while (n--) *--d = *--s; }
}
static void PelFill(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = (uint8_t)c;
}
static int PelCmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static void *LAlloc(PefFragment *f, size_t n)
{
    return f->alloc ? f->alloc(f->allocCtx, n) : NULL;
}
static void LFree(PefFragment *f, void *p)
{
    if (f->free_ && p) f->free_(f->allocCtx, p);
}

/* ---------------------------------------------------------------- parsing */

static int PefParseHeaders(PefFragment *f)
{
    const uint8_t *b = f->file;
    size_t base = f->base;
    int i;

    f->formatVersion    = rd32(b + base + 12);
    f->sectionCount     = rd16(b + base + 32);
    f->instSectionCount = rd16(b + base + 34);
    if (f->sectionCount == 0 || f->sectionCount > 64)
        return -1;

    f->sections = (PefSectionHeader *)LAlloc(f, (size_t)f->sectionCount * sizeof(*f->sections));
    if (!f->sections)
        return -1;
    PelFill(f->sections, 0, (size_t)f->sectionCount * sizeof(*f->sections));

    for (i = 0; i < f->sectionCount; i++) {
        const uint8_t *p = b + base + 40 + 28 * i;
        PefSectionHeader *s = &f->sections[i];
        s->nameOffset      = rds32(p + 0);
        s->defaultAddress  = rd32(p + 4);
        s->totalLength     = rd32(p + 8);
        s->unpackedLength  = rd32(p + 12);
        s->containerLength = rd32(p + 16);
        s->containerOffset = rd32(p + 20);
        s->sectionKind     = p[24];
        s->shareKind       = p[25];
        s->alignment       = p[26];
        s->reservedA       = p[27];
        if (s->sectionKind == kPefLoaderSection)
            f->loaderSection = i;
        if ((size_t)s->containerOffset + s->containerLength > f->fileLen - base)
            return -1;
        if ((size_t)s->containerOffset + s->containerLength > f->end - base)
            f->end = base + s->containerOffset + s->containerLength;
    }
    return 0;
}

static void PefResolveLibraryNames(PefFragment *f)
{
    uint32_t i;
    for (i = 0; i < f->libCount; i++) {
        const uint8_t *p = f->file + f->stringsBase + f->libs[i].nameOffset;
        size_t n = 0;
        while (p + n < f->file + f->fileLen && p[n] && n < sizeof(f->libs[i].name) - 1) {
            f->libs[i].name[n] = (char)p[n];
            n++;
        }
        f->libs[i].name[n] = '\0';
    }
}

static int PefParseLoader(PefFragment *f)
{
    const uint8_t *b = f->file;
    size_t L;
    uint32_t i;
    const uint8_t *p;

    if (f->loaderSection < 0)
        return -1;
    f->loaderBase = f->base + f->sections[f->loaderSection].containerOffset;
    L = f->loaderBase;

    f->mainSection = rds32(b + L + 0);
    f->mainOffset  = rd32(b + L + 4);
    f->initSection = rds32(b + L + 8);
    f->initOffset  = rd32(b + L + 12);
    f->termSection = rds32(b + L + 16);
    f->termOffset  = rd32(b + L + 20);
    f->libCount    = rd32(b + L + 24);
    f->symCount    = rd32(b + L + 28);
    f->relocSectionCount = rd32(b + L + 32);
    f->relocInstrOffset  = rd32(b + L + 36);
    f->loaderStringsOffset = rd32(b + L + 40);
    f->exportHashOffset    = rd32(b + L + 44);
    f->hashPower           = rd32(b + L + 48);
    f->exportCount         = rd32(b + L + 52);
    f->stringsBase = L + f->loaderStringsOffset;

    if (f->libCount > 4096 || f->symCount > 262144 || f->exportCount > 262144)
        return -1;

    f->libs = (PefImportedLibrary *)LAlloc(f, (size_t)(f->libCount ? f->libCount : 1) * sizeof(*f->libs));
    f->imports = (PefImportedSymbol *)LAlloc(f, (size_t)(f->symCount ? f->symCount : 1) * sizeof(*f->imports));
    f->exports = (PefExportedSymbol *)LAlloc(f, (size_t)(f->exportCount ? f->exportCount : 1) * sizeof(*f->exports));
    if (!f->libs || !f->imports || !f->exports)
        return -1;
    PelFill(f->libs, 0, (size_t)(f->libCount ? f->libCount : 1) * sizeof(*f->libs));
    PelFill(f->imports, 0, (size_t)(f->symCount ? f->symCount : 1) * sizeof(*f->imports));
    PelFill(f->exports, 0, (size_t)(f->exportCount ? f->exportCount : 1) * sizeof(*f->exports));

    p = b + L + 56;
    for (i = 0; i < f->libCount; i++, p += 24) {
        f->libs[i].nameOffset         = rd32(p + 0);
        f->libs[i].oldImpVersion      = rd32(p + 4);
        f->libs[i].currentVersion     = rd32(p + 8);
        f->libs[i].importedSymbolCount = rd32(p + 12);
        f->libs[i].firstImportedSymbol = rd32(p + 16);
        f->libs[i].options            = p[20];
        f->libs[i].reservedA          = p[21];
        f->libs[i].reservedB          = rd16(p + 22);
    }
    for (i = 0; i < f->symCount; i++, p += 4) {
        uint32_t cn = rd32(p);
        f->imports[i].cls = (uint8_t)(cn >> 24);
        f->imports[i].nameOffset = cn & 0x00FFFFFFu;
        f->imports[i].name = (const char *)(b + f->stringsBase + f->imports[i].nameOffset);
    }
    PefResolveLibraryNames(f);

    /* Export names are stored back-to-back without NUL terminators; the length
       lives in the upper 16 bits of the parallel key-table entry. */
    {
        size_t area = L + f->exportHashOffset;
        const uint8_t *keys = b + area + ((size_t)1 << f->hashPower) * 4;
        const uint8_t *syms = keys + (size_t)f->exportCount * 4;
        for (i = 0; i < f->exportCount; i++) {
            const uint8_t *sp = syms + 10 * i;
            uint32_t cn = rd32(sp);
            uint32_t klen = rd32(keys + 4 * i) >> 16;
            char *nm;
            size_t off = cn & 0x00FFFFFFu;
            if (klen > 4096)
                klen = 4096;
            nm = (char *)LAlloc(f, klen + 1);
            if (!nm)
                return -1;
            PelMove(nm, b + f->stringsBase + off, klen);
            nm[klen] = '\0';
            f->exports[i].cls = (uint8_t)(cn >> 24);
            f->exports[i].name = nm;
            f->exports[i].symbolValue = rd32(sp + 4);
            f->exports[i].sectionIndex = rds16(sp + 8);
        }
    }
    return 0;
}

int PefParse(PefFragment *f, const uint8_t *file, size_t fileLen, size_t base)
{
    PefAllocFn alloc = f ? f->alloc : NULL;
    PefFreeFn  free_ = f ? f->free_ : NULL;
    void      *ctx   = f ? f->allocCtx : NULL;
    PelFill(f, 0, sizeof(*f));
    f->alloc = alloc;
    f->free_ = free_;
    f->allocCtx = ctx;
    f->file = file;
    f->fileLen = fileLen;
    f->base = base;
    f->end = base + 40;
    f->loaderSection = -1;
    if (base + 40 > fileLen)
        return -1;
    if (rd32(file + base + 0) != PEF_CONTAINER_TAG0 ||
        rd32(file + base + 4) != PEF_CONTAINER_TAG1)
        return -1;
    if (PefParseHeaders(f) != 0)
        return -1;
    if (PefParseLoader(f) != 0)
        return -1;
    return 0;
}

void PefFree(PefFragment *f)
{
    uint32_t i;
    if (!f->alloc)
        return;
    if (f->exports) {
        for (i = 0; i < f->exportCount; i++)
            LFree(f, (void *)f->exports[i].name);
    }
    if (f->expanded) {
        for (i = 0; i < f->sectionCount; i++)
            LFree(f, f->expanded[i]);
    }
    LFree(f, f->sections);
    LFree(f, f->libs);
    LFree(f, f->imports);
    LFree(f, f->exports);
    LFree(f, f->expanded);
    PelFill(f, 0, sizeof(*f));
}

int PefEnumFragments(const uint8_t *file, size_t fileLen,
                     PefFragment *frags, int maxFrags)
{
    size_t base = 0;
    int n = 0;

    /* Find the first full container tag. */
    for (base = 0; base + 12 <= fileLen; base++) {
        if (rd32(file + base) == PEF_CONTAINER_TAG0 &&
            rd32(file + base + 4) == PEF_CONTAINER_TAG1 &&
            rd32(file + base + 8) == 0x70777063u /* 'pwpc' */)
            break;
    }
    while (base + 40 <= fileLen && n < maxFrags) {
        if (rd32(file + base) != PEF_CONTAINER_TAG0 ||
            rd32(file + base + 4) != PEF_CONTAINER_TAG1)
            break;
        if (PefParse(&frags[n], file, fileLen, base) != 0) {
            PefFree(&frags[n]);
            break;
        }
        {
            size_t next = (frags[n].end + 15) & ~(size_t)15;
            if (next + 12 > fileLen ||
                rd32(file + next) != PEF_CONTAINER_TAG0) {
                /* tolerate padding between containers */
                size_t scan;
                for (scan = frags[n].end; scan + 12 <= fileLen; scan++) {
                    if (rd32(file + scan) == PEF_CONTAINER_TAG0 &&
                        rd32(file + scan + 4) == PEF_CONTAINER_TAG1 &&
                        rd32(file + scan + 8) == 0x70777063u) {
                        next = scan;
                        break;
                    }
                }
                if (scan + 12 > fileLen)
                    next = fileLen;
            }
            base = next;
        }
        n++;
    }
    return n;
}

/* --------------------------------------------------- pattern-initialized */

static uint32_t PefVarCount(const uint8_t *p, size_t *pos, size_t limit, int *ok)
{
    uint32_t v = 0;
    for (;;) {
        uint8_t c;
        if (*pos >= limit) { *ok = 0; return v; }
        c = p[(*pos)++];
        v = (v << 7) | (c & 0x7F);
        if (!(c & 0x80))
            break;
    }
    return v;
}

static int PefExpandOne(const uint8_t *src, size_t srcLen, uint8_t *dst,
                        size_t dstLen)
{
    size_t o = 0, p = 0;
    while (p < srcLen) {
        uint8_t inst = src[p++];
        unsigned op = inst >> 5;
        uint32_t cnt = inst & 0x1F;
        int ok = 1;
        if (cnt == 0 && op != 0) {
            cnt = PefVarCount(src, &p, srcLen, &ok);
            if (!ok) return -1;
        } else if (cnt == 0) {
            cnt = PefVarCount(src, &p, srcLen, &ok);
            if (!ok) return -1;
        }
        switch (op) {
        case 0: /* zero */
            if (o + cnt > dstLen) return -1;
            PelFill(dst + o, 0, cnt);
            o += cnt;
            break;
        case 1: /* block copy */
            if (p + cnt > srcLen || o + cnt > dstLen) return -1;
            PelMove(dst + o, src + p, cnt);
            p += cnt; o += cnt;
            break;
        case 2: { /* repeat */
            uint32_t rep = PefVarCount(src, &p, srcLen, &ok);
            if (!ok || p + cnt > srcLen) return -1;
            if (o + (size_t)cnt * (rep + 1) > dstLen) return -1;
            for (uint32_t r = 0; r <= rep; r++) {
                PelMove(dst + o, src + p, cnt);
                o += cnt;
            }
            p += cnt;
            break;
        }
        case 3: { /* interleave repeated block with block copy */
            uint32_t common = cnt;
            uint32_t custom = PefVarCount(src, &p, srcLen, &ok);
            uint32_t rep;
            if (!ok) return -1;
            rep = PefVarCount(src, &p, srcLen, &ok);
            if (!ok || p + common + (size_t)custom * rep > srcLen) return -1;
            if (o + (size_t)(common + custom) * rep + common > dstLen) return -1;
            {
                const uint8_t *commonData = src + p;
                p += common;
                for (uint32_t r = 0; r < rep; r++) {
                    PelMove(dst + o, commonData, common); o += common;
                    PelMove(dst + o, src + p, custom); o += custom;
                    p += custom;
                }
                PelMove(dst + o, commonData, common); o += common;
            }
            break;
        }
        case 4: { /* interleave repeated block with zero */
            uint32_t common = cnt;
            uint32_t custom = PefVarCount(src, &p, srcLen, &ok);
            uint32_t rep;
            if (!ok) return -1;
            rep = PefVarCount(src, &p, srcLen, &ok);
            if (!ok || p + (size_t)custom * rep > srcLen) return -1;
            if (o + (size_t)(common + custom) * rep + common > dstLen) return -1;
            for (uint32_t r = 0; r < rep; r++) {
                PelFill(dst + o, 0, common); o += common;
                PelMove(dst + o, src + p, custom); o += custom;
                p += custom;
            }
            PelFill(dst + o, 0, common); o += common;
            break;
        }
        default:
            return -1;
        }
    }
    return 0;
}

int PefExpandSections(PefFragment *f)
{
    int i;
    if (f->expanded)
        return 0;
    f->expanded = (uint8_t **)LAlloc(f, (size_t)f->sectionCount * sizeof(*f->expanded));
    if (!f->expanded)
        return -1;
    PelFill(f->expanded, 0, (size_t)f->sectionCount * sizeof(*f->expanded));
    for (i = 0; i < f->sectionCount; i++) {
        PefSectionHeader *s = &f->sections[i];
        switch (s->sectionKind) {
        case kPefCodeSection:
        case kPefUnpackedDataSection:
        case kPefConstantSection:
        case kPefExecDataSection:
            f->expanded[i] = (uint8_t *)LAlloc(f, s->totalLength ? s->totalLength : 1);
            if (!f->expanded[i]) return -1;
            PelFill(f->expanded[i], 0, s->totalLength);
            PelMove(f->expanded[i], f->file + f->base + s->containerOffset,
                    s->unpackedLength);
            break;
        case kPefPackedDataSection:
            f->expanded[i] = (uint8_t *)LAlloc(f, s->totalLength ? s->totalLength : 1);
            if (!f->expanded[i]) return -1;
            PelFill(f->expanded[i], 0, s->totalLength);
            if (PefExpandOne(f->file + f->base + s->containerOffset,
                             s->containerLength, f->expanded[i],
                             s->totalLength) != 0) {
                return -1;
            }
            break;
        default:
            break;
        }
    }
    return 0;
}

/* ---------------------------------------------------------- placement */

uint32_t PefPlaceFragment(PefFragment *f, uint32_t codeBase, uint32_t dataBase)
{
    uint32_t codeCur = codeBase, dataCur = dataBase;
    int i;
    for (i = 0; i < f->sectionCount; i++) {
        PefSectionHeader *s = &f->sections[i];
        uint32_t align = (s->alignment <= 15) ? (1u << s->alignment) : 16u;
        uint32_t sz = s->totalLength;
        if (s->sectionKind == kPefCodeSection || s->sectionKind == kPefExecDataSection) {
            codeCur = (codeCur + align - 1) & ~(align - 1);
            f->secBase[i] = codeCur;
            codeCur += sz;
        } else if (s->sectionKind == kPefUnpackedDataSection ||
                   s->sectionKind == kPefPackedDataSection ||
                   s->sectionKind == kPefConstantSection) {
            dataCur = (dataCur + align - 1) & ~(align - 1);
            f->secBase[i] = dataCur;
            dataCur += sz;
        } else {
            f->secBase[i] = 0;
        }
    }
    /* TOC: second word of the init transition vector (defaultAddress 0). */
    f->toc = 0;
    if (f->initSection >= 0 && f->initSection < f->sectionCount &&
        f->expanded[f->initSection]) {
        PefSectionHeader *s = &f->sections[f->initSection];
        uint32_t raw = rd32(f->expanded[f->initSection] + f->initOffset + 4);
        f->toc = f->secBase[f->initSection] + (raw - s->defaultAddress);
    }
    return f->toc;
}

uint32_t PefExportAddress(const PefFragment *f, const PefExportedSymbol *e)
{
    if (e->sectionIndex < 0 || (int)e->sectionIndex >= f->sectionCount)
        return 0;
    if (!f->expanded[e->sectionIndex])
        return 0;
    return f->secBase[e->sectionIndex] +
           (e->symbolValue - f->sections[e->sectionIndex].defaultAddress);
}

const PefExportedSymbol *PefFindExport(PefFragment *frags, int nFrags,
                                       const char *name, int *outFrag)
{
    int i, k;
    for (i = 0; i < nFrags; i++) {
        for (k = 0; k < (int)frags[i].exportCount; k++) {
            if (frags[i].exports[k].name &&
                PelCmp(frags[i].exports[k].name, name) == 0) {
                if (outFrag) *outFrag = i;
                return &frags[i].exports[k];
            }
        }
    }
    return NULL;
}

int PefImportIndex(const PefFragment *f, const char *name)
{
    uint32_t i;
    for (i = 0; i < f->symCount; i++) {
        if (f->imports[i].name && PelCmp(f->imports[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

/* --------------------------------------------------------- relocation */

#define PEF_RELOC_MAX_CHUNKS 65536

typedef struct {
    const uint8_t *chunks; /* big-endian 16-bit words */
    size_t nchunks;
} PefChunkStream;

static uint16_t PefChunkAt(const PefChunkStream *s, size_t i)
{
    if (i >= s->nchunks) return 0;
    return rd16(s->chunks + 2 * i);
}

static unsigned PefField(uint16_t chunk, int off, int len)
{
    return (chunk >> (16 - (off + len))) & ((1u << len) - 1);
}

static int PefExecRange(const PefChunkStream *cs, size_t start, size_t count,
                        int secDefault, uint32_t *relocAddr, int *importIndex,
                        int curSection, uint32_t *secBase, uint32_t *importAddrs,
                        uint8_t *buf, size_t bufLen)
{
    size_t pc = start, end = start + count;
    while (pc < end) {
        uint16_t chunk = PefChunkAt(cs, pc);
        unsigned op = chunk >> 9;
        size_t i;

        if (op < 0x20) { /* RelocBySectDWithSkip */
            unsigned skip = PefField(chunk, 2, 8);
            unsigned run = PefField(chunk, 10, 6);
            *relocAddr += skip * 4;
            for (i = 0; i < run; i++) {
                size_t p = (*relocAddr - secDefault) + 4 * i;
                if (p + 4 > bufLen) return -1;
                { uint32_t v = rd32(buf + p) + secBase[1]; PelMove(buf + p, &(uint8_t[4]){v>>24,v>>16,v>>8,v}, 4); }
            }
            *relocAddr += run * 4;
            pc += 1;
        } else if (op <= 0x25) { /* RelocRun group */
            unsigned sub = PefField(chunk, 3, 4);
            unsigned run = PefField(chunk, 7, 9) + 1;
            switch (sub) {
            case 0: /* BySectC */
                for (i = 0; i < run; i++) {
                    size_t p = (*relocAddr - secDefault) + 4 * i;
                    uint32_t v;
                    if (p + 4 > bufLen) return -1;
                    v = rd32(buf + p) + secBase[0];
                    PelMove(buf + p, &(uint8_t[4]){v>>24,v>>16,v>>8,v}, 4);
                }
                *relocAddr += run * 4; break;
            case 1: /* BySectD */
                for (i = 0; i < run; i++) {
                    size_t p = (*relocAddr - secDefault) + 4 * i;
                    uint32_t v;
                    if (p + 4 > bufLen) return -1;
                    v = rd32(buf + p) + secBase[1];
                    PelMove(buf + p, &(uint8_t[4]){v>>24,v>>16,v>>8,v}, 4);
                }
                *relocAddr += run * 4; break;
            case 2: /* TVector12 */
                for (i = 0; i < run; i++) {
                    size_t p = (*relocAddr - secDefault) + 12 * i;
                    uint32_t a, b;
                    if (p + 12 > bufLen) return -1;
                    a = rd32(buf + p) + secBase[0];
                    b = rd32(buf + p + 4) + secBase[1];
                    PelMove(buf + p, &(uint8_t[4]){a>>24,a>>16,a>>8,a}, 4);
                    PelMove(buf + p + 4, &(uint8_t[4]){b>>24,b>>16,b>>8,b}, 4);
                }
                *relocAddr += run * 12; break;
            case 3: /* TVector8 */
                for (i = 0; i < run; i++) {
                    size_t p = (*relocAddr - secDefault) + 8 * i;
                    uint32_t a, b;
                    if (p + 8 > bufLen) return -1;
                    a = rd32(buf + p) + secBase[0];
                    b = rd32(buf + p + 4) + secBase[1];
                    PelMove(buf + p, &(uint8_t[4]){a>>24,a>>16,a>>8,a}, 4);
                    PelMove(buf + p + 4, &(uint8_t[4]){b>>24,b>>16,b>>8,b}, 4);
                }
                *relocAddr += run * 8; break;
            case 4: /* VTable8 */
                for (i = 0; i < run; i++) {
                    size_t p = (*relocAddr - secDefault) + 8 * i;
                    uint32_t a;
                    if (p + 8 > bufLen) return -1;
                    a = rd32(buf + p) + secBase[1];
                    PelMove(buf + p, &(uint8_t[4]){a>>24,a>>16,a>>8,a}, 4);
                }
                *relocAddr += run * 8; break;
            case 5: /* ImportRun */
                for (i = 0; i < run; i++) {
                    size_t p = (*relocAddr - secDefault) + 4 * i;
                    uint32_t v;
                    if (p + 4 > bufLen) return -1;
                    v = rd32(buf + p) + importAddrs[*importIndex + i];
                    PelMove(buf + p, &(uint8_t[4]){v>>24,v>>16,v>>8,v}, 4);
                }
                *importIndex += run;
                *relocAddr += run * 4; break;
            }
            pc += 1;
        } else if (op <= 0x33) { /* RelocSmIndex group */
            unsigned sub = PefField(chunk, 3, 4);
            unsigned idx = PefField(chunk, 7, 9);
            if (sub == 0) { /* SmByImport */
                size_t p = *relocAddr - secDefault;
                uint32_t v;
                if (p + 4 > bufLen) return -1;
                v = rd32(buf + p) + importAddrs[idx];
                PelMove(buf + p, &(uint8_t[4]){v>>24,v>>16,v>>8,v}, 4);
                *importIndex = idx + 1;
                *relocAddr += 4;
            } else if (sub == 1) { /* SmSetSectC */
                curSection = idx;
                (void)curSection;
            } else if (sub == 2) { /* SmSetSectD */
                /* sectionD is always the data section in this model */
            } else { /* SmBySection */
                size_t p = *relocAddr - secDefault;
                uint32_t v;
                if (p + 4 > bufLen) return -1;
                v = rd32(buf + p) + secBase[idx];
                PelMove(buf + p, &(uint8_t[4]){v>>24,v>>16,v>>8,v}, 4);
                *relocAddr += 4;
            }
            pc += 1;
        } else if (op >= 0x40 && op <= 0x47) { /* RelocIncrPosition */
            *relocAddr += PefField(chunk, 4, 12) + 1;
            pc += 1;
        } else if (op >= 0x48 && op <= 0x4F) { /* RelocSmRepeat */
            unsigned cc = PefField(chunk, 4, 4) + 1;
            unsigned rc = PefField(chunk, 8, 8) + 1;
            size_t blkStart = pc - cc;
            unsigned r;
            for (r = 0; r < rc; r++) {
                if (PefExecRange(cs, blkStart, cc, secDefault, relocAddr,
                                 importIndex, curSection, secBase, importAddrs,
                                 buf, bufLen) != 0)
                    return -1;
            }
            pc += 1;
        } else if (op == 0x50) { /* RelocSetPosition */
            unsigned hi = PefField(chunk, 6, 10);
            uint32_t off = ((uint32_t)hi << 16) | PefChunkAt(cs, pc + 1);
            *relocAddr = secDefault + off;
            pc += 2;
        } else if (op == 0x52) { /* RelocLgByImport */
            unsigned hi = PefField(chunk, 6, 10);
            uint32_t idx = ((uint32_t)hi << 16) | PefChunkAt(cs, pc + 1);
            size_t p = *relocAddr - secDefault;
            uint32_t v;
            if (p + 4 > bufLen || idx >= (1u << 20)) return -1;
            v = rd32(buf + p) + importAddrs[idx];
            PelMove(buf + p, &(uint8_t[4]){v>>24,v>>16,v>>8,v}, 4);
            *importIndex = (int)idx + 1;
            *relocAddr += 4;
            pc += 2;
        } else if (op == 0x58) { /* RelocLgRepeat */
            unsigned cc = PefField(chunk, 6, 4) + 1;
            uint32_t rc = ((uint32_t)PefField(chunk, 10, 6) << 16) | PefChunkAt(cs, pc + 1);
            size_t blkStart = pc - cc;
            uint32_t r;
            for (r = 0; r < rc; r++) {
                if (PefExecRange(cs, blkStart, cc, secDefault, relocAddr,
                                 importIndex, curSection, secBase, importAddrs,
                                 buf, bufLen) != 0)
                    return -1;
            }
            pc += 2;
        } else if (op == 0x5A) { /* RelocLgSetOrBySection */
            unsigned sub = PefField(chunk, 6, 4);
            uint32_t idx = ((uint32_t)PefField(chunk, 10, 6) << 16) | PefChunkAt(cs, pc + 1);
            if (sub == 0) {
                size_t p = *relocAddr - secDefault;
                uint32_t v;
                if (p + 4 > bufLen || idx >= 64) return -1;
                v = rd32(buf + p) + secBase[idx];
                PelMove(buf + p, &(uint8_t[4]){v>>24,v>>16,v>>8,v}, 4);
                *relocAddr += 4;
            }
            /* sub 1/2 set sectionC/D; unused in this model */
            pc += 2;
        } else {
            return -1;
        }
    }
    return 0;
}

int PefRelocateSection(PefFragment *f, int secIndex, uint8_t *buf,
                       uint32_t *importAddrs)
{
    size_t hdrBase, relocBase;
    uint32_t h;

    if (!f->expanded[secIndex])
        return -1;
    hdrBase = f->loaderBase + 56 + 24 * f->libCount + 4 * f->symCount;
    relocBase = f->loaderBase + f->relocInstrOffset;

    for (h = 0; h < f->relocSectionCount; h++) {
        const uint8_t *rh = f->file + hdrBase + 12 * h;
        uint16_t sIdx = rd16(rh + 0);
        uint32_t cnt = rd32(rh + 4);
        uint32_t first = rd32(rh + 8);
        if (sIdx != (uint16_t)secIndex)
            continue;
        {
            PefChunkStream cs;
            uint32_t relocAddr = f->sections[secIndex].defaultAddress;
            int importIndex = 0;
            cs.chunks = f->file + relocBase + first;
            cs.nchunks = cnt;
            if (PefExecRange(&cs, 0, cnt, f->sections[secIndex].defaultAddress,
                             &relocAddr, &importIndex, 0, f->secBase,
                             importAddrs, buf,
                             f->sections[secIndex].totalLength) != 0)
                return -1;
        }
    }
    return 0; /* no header == nothing to relocate */
}
