# First mission engine investigation

The Pit is the first target. Its installed zone is so_killspree_trainer.ff. A timed target course gives testers a visible shared start, target state and finish. Spawn, restart and completion script ownership still require verification.

On the supported retail image, the scan at RVA 0x22ed40 tests the leading signed dword against 2, advances by 0x1678 and stops at the address immediately after the second server-client record. The paired walk at 0x22f020 advances a server-client pointer by 0x1678 and a connection-state pointer by 0x2650. The comparison at 0x22f055 uses the server-client end pointer. In the tested branch, the connection-state field has offset 0x10 and the client record also contributes a field at 0x1210. The fixture supplies separate record buffers and an expanded end pointer directly to the loop body. The retail preamble still selects its original storage.

Treat the connection-table end address with care. At 0xcd35c and 0xd5487, instructions load that address and add an index scaled by 0x3218. At 0xd5491 the resulting pointer is dereferenced at offset 0x2010. These references access adjacent storage. Replacing every matching address with a new end pointer would redirect those accesses. The type and ownership of that adjacent storage remain unresolved.

The next engine dependency is the allocation, initialization and teardown path for each paired table, followed by the link from an admitted connection to a separately owned game entity. Validate every address consumer by behavior before enabling a running session. The separate lobby slot index supplies no evidence for either dependency.

Run tools/check_engine_routines.py with Python, pefile 2024.8.26, unicorn 2.1.4 and the path to the supported iw4sp.exe. It reads the retail image and executes bounded routines inside an emulator. It checks the actual bit helpers with two-bit widths, the unmodified two-record scan, and the paired loop body with four fixture records. The existing native codec checker covers the call-site changes on a private image.
