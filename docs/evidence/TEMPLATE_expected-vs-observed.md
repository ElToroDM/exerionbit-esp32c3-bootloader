# Expected vs Observed - <release>

Scope: <tests included in this release evidence set>.

## Environment

- Date: <YYYY-MM-DD>
- Release context: <tag or commit id>
- Board: <target board>
- Connection: <serial mode and port>
- Baud: <baud rate>
- ESP-IDF: <version>
- Host Python: <python path or version>
- esptool: <version>

## Commands used

- Build: `<command>`
- Flash: `<command>`
- Monitor/capture: `<command>`
- Extra validators: `<command>`

## <Test case name>

- Log: `docs/evidence/<release>/logs/<file>.log`
- Expected: <ordered token sequence or behavior contract>
- Observed: <actual result>
- Status: `PASS` | `FAIL` | `PARTIAL`
- Rationale: <short technical reason>

## Notes

- Residual risks: <if any>
- Follow-up actions: <if status is PARTIAL or FAIL>
