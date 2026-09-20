# X4 Pro EPUB page working set

The reader keeps serialized page payloads for the current page, two previous pages,
and eight following pages in PSRAM. One allocation holds eleven 32 KiB slots and
their metadata (360,636 bytes total). It is allocated on the first idle fill of a
section and freed when that section is destroyed. There is no internal-memory
fallback for this allocation; failure disables this tier for that section.
Oversized pages bypass it and continue loading from SD.

The SD section cache remains authoritative, with no format/version changes.
Completed payloads deserialize through the same Page/TextBlock/ImageBlock code as
SD loads. Their object allocations are unchanged. Dictionary lookup still calls
the existing SD page loader. The seven-frame rendered cache remains in place.

Idle work starts after the existing 400 ms reader debounce, after input handling,
and reads at most 1 KiB of page payload per loop pass. A separate metadata pass
opens the file and reads its page offsets. Requested turns never finish or wait
for an incomplete preload; they release its handle and use SD on a miss. An SD
operation already in flight still takes as long as the card requires. Completed
build pages are read through a separate HAL handle after flushing the writer.
The handle's small existing internal-heap wrapper is released at fill completion,
failure, cancellation, or invalidation.

Section teardown, reload, rebuild, file replacement, and settings/orientation
changes invalidate the working set. A newly built page invalidates its old slot,
including when a partial chapter is being extended. No decoded Page graph stays
resident between loads.

## Verification

Build with `pio run -e x4pro`. Native checks:

```sh
cmake --build build/test --target PsramBufferTest ChapterHtmlSlimParserTest -j 4
ctest --test-dir build/test -R 'PsramBufferTest|ChapterHtmlSlimParserTest' --output-on-failure
```

On X4 Pro with debug logging, open a cached chapter and allow idle filling. Look
for `Idle page data`, then turn forward/back within the window. `Page data PSRAM`
reports hits, SD page loads avoided, misses, evictions, allocated PSRAM bytes, and
deserialization time. Those counters include framebuffer idle preparation and
start when the data cache is enabled. `Page data SD` reports load plus deserialize
time; `SD deserialization` isolates its decode time. `Page turn framebuffer-hit`,
`Page turn PSRAM-data-hit`, and `Page turn SD-data-load` distinguish turn paths.
Framebuffer hits still load page metadata for links, footnotes, and progress.

Test chapter jumps, another book, cache clearing, typography changes, all four
orientations, partial-chapter extension, and rapid turns during idle fills. Check
text, images, links, footnotes, and saved position against SD-only behavior.

For a long-session comparison, use the same book/settings on the previous and new
firmware, warm the fonts, and repeatedly traverse the same pages. Compare
`After page turn: internal-free=... internal-largest=...` at the same positions;
watch for a downward trend and check PSRAM recovery on book exit. Slot allocation
stays fixed as the window moves. Hardware heap parity, responsiveness, and leak
behavior require this device check; host tests do not establish those results.
