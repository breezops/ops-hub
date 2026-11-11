# Loop-1 · OPS 0.1.21

- The host's in-game Restart Mission command creates a new mission epoch in
  the same party. Guests request the existing deferred native restart once.
- The room resumes after each PC sees its scene clear and local player return.
- Logs record the queued guest restart, native scene clear and room resumption.
- This movement build isolates the restart path for live validation.
