/*
 * pef_loader.h - PEF / CFM fragment loader for the classic Mac OS runtime.
 *
 * Parses PEF ("Preferred Executable Format") containers out of a Mac OS data
 * fork, expands pattern-initialized data, applies the CFM relocation bytecode
 * and resolves imported symbols by name.  See "Mac OS Runtime Architectures",
 * Chapter 8, for the on-disk formats; the physical layouts implemented here
 * were cross-checked against Apple's PEFBinaryFormat.h.
 *
 * The module is deliberately free of emulator dependencies (it only uses
 * <stdint.h>) so it can be unit-tested on the host.  Integration with guest
 * memory is done by the caller through PefPlaceFragment()/PefRelocateSection().
 */
#ifndef PEF_LOADER_H
#define PEF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#define PEF_CONTAINER_TAG0 0x4A6F7921u /* 'Joy!' */
#define PEF_CONTAINER_TAG1 0x70656666u /* 'peff' */

/* The loader is freestanding: it never calls libc.  All scratch memory goes
   through these callbacks, which must be set on the fragment before PefParse.
   `alloc` returns at least n bytes (or NULL on failure); `free_` releases a
   pointer previously returned by alloc (may be NULL for bump allocators). */
typedef void *(*PefAllocFn)(void *ctx, size_t n);
typedef void  (*PefFreeFn)(void *ctx, void *p);

enum {
    kPefCodeSection         = 0,
    kPefUnpackedDataSection = 1,
    kPefPackedDataSection   = 2,
    kPefConstantSection     = 3,
    kPefLoaderSection       = 4,
    kPefExecDataSection     = 6
};

enum {
    kPefCodeSymbol    = 0x00,
    kPefDataSymbol    = 0x01,
    kPefTVectorSymbol = 0x02,
    kPefTOCSymbol     = 0x03,
    kPefGlueSymbol    = 0x04,
    kPefUndefinedSymbol = 0x0F,
    kPefWeakImportSymMask = 0x80
};

enum {
    kPefWeakImportLibMask  = 0x40,
    kPefInitLibBeforeMask  = 0x80
};

typedef struct {
    int32_t  nameOffset;      /* -1 => no name */
    uint32_t defaultAddress;
    uint32_t totalLength;
    uint32_t unpackedLength;
    uint32_t containerLength;
    uint32_t containerOffset;
    uint8_t  sectionKind;
    uint8_t  shareKind;
    uint8_t  alignment;
    uint8_t  reservedA;
} PefSectionHeader;

typedef struct {
    uint32_t nameOffset;
    uint32_t oldImpVersion;
    uint32_t currentVersion;
    uint32_t importedSymbolCount;
    uint32_t firstImportedSymbol;
    uint8_t  options;
    uint8_t  reservedA;
    uint16_t reservedB;
    char     name[64];        /* resolved library name (informational) */
} PefImportedLibrary;

typedef struct {
    uint8_t  cls;             /* kPef*Symbol, may have 0x80 weak bit */
    uint32_t nameOffset;
    const char *name;         /* points into string table (NUL terminated) */
} PefImportedSymbol;

typedef struct {
    uint8_t  cls;
    const char *name;         /* points into string table (NOT NUL terminated) */
    uint32_t symbolValue;
    int16_t  sectionIndex;    /* negative => pseudo-section */
} PefExportedSymbol;

typedef struct {
    const uint8_t *file;
    size_t   fileLen;
    size_t   base;            /* byte offset of this container in file */
    size_t   end;             /* one past the last container byte */

    /* Memory callbacks (set before PefParse; if alloc is NULL the loader
       cannot allocate and PefParse returns -1). */
    PefAllocFn alloc;
    PefFreeFn  free_;
    void      *allocCtx;

    uint32_t formatVersion;
    uint16_t sectionCount;
    uint16_t instSectionCount;
    PefSectionHeader *sections;
    int      loaderSection;   /* index of the kind-4 section, or -1 */

    int32_t  mainSection, initSection, termSection;
    uint32_t mainOffset, initOffset, termOffset;
    uint32_t libCount, symCount, relocSectionCount;
    uint32_t relocInstrOffset, loaderStringsOffset, exportHashOffset;
    uint32_t hashPower, exportCount;

    size_t   loaderBase;      /* base + loader section containerOffset */
    size_t   stringsBase;     /* loaderBase + loaderStringsOffset */

    PefImportedLibrary *libs;
    PefImportedSymbol  *imports;
    PefExportedSymbol  *exports;

    /* Host image of each instantiated section, expanded to totalLength.
       expanded[i] is NULL for non-instantiated sections. */
    uint8_t **expanded;

    /* Fragment runtime placement (filled by PefPlaceFragment). */
    uint32_t secBase[16];
    uint32_t toc;             /* value the code's r2 must hold */
} PefFragment;

/* Walk every contiguous PEF container in [file, file+fileLen).  Fills up to
   maxFrags entries and returns the number found. */
int PefEnumFragments(const uint8_t *file, size_t fileLen,
                     PefFragment *frags, int maxFrags);

/* Parse a single container starting at `base`.  Returns 0 on success, -1 on
   malformed input.  Does not expand sections (call PefExpandSections). */
int  PefParse(PefFragment *f, const uint8_t *file, size_t fileLen, size_t base);
void PefFree(PefFragment *f);

/* Expand all packed (kind 2) sections into f->expanded[i]. */
int  PefExpandSections(PefFragment *f);

/* Choose per-section runtime bases and compute the fragment TOC.  codeBase is
   used for code sections (kind 0/6), dataBase for the first data section and
   subsequent writable ones are packed after it.  Returns the TOC value. */
uint32_t PefPlaceFragment(PefFragment *f, uint32_t codeBase, uint32_t dataBase);

/* Apply the relocation bytecode for one section to `buf` (a host image of the
   section).  importAddrs[i] is the runtime address of imported symbol i; an
   address of 0 leaves the reference alone.  Returns 0 on success. */
int PefRelocateSection(PefFragment *f, int secIndex, uint8_t *buf,
                       uint32_t *importAddrs);

/* Resolve the runtime address of an exported symbol within a placed fragment.
   Returns 0 if the export does not live in an instantiated section. */
uint32_t PefExportAddress(const PefFragment *f, const PefExportedSymbol *e);

/* Find an export by name across `frags`; returns the owning fragment index via
   *outFrag (may be NULL) and the export pointer, or NULL. */
const PefExportedSymbol *PefFindExport(PefFragment *frags, int nFrags,
                                       const char *name, int *outFrag);

/* Find an imported symbol by name (NUL-terminated) and return its index, or
   -1 if not found. */
int PefImportIndex(const PefFragment *f, const char *name);

#endif /* PEF_LOADER_H */
