# Manual test table: order screen & roles (Member 1)

Tester: ________   Computer: ________   Date: ________

| # | Step | Expected clicks | Expected result | Pass / Fail |
|---|------|-----------------|-----------------|-------------|
| 1 | Scan (barcode box + Enter or camera) | 0 extra clicks | Order starts automatically (Walk-in if customer blank), item added with qty 1, qty box returns to 1, barcode box clears & keeps focus, totals update via `buildBill()`, green status | [ ] |
| 2 | Repeat scan | 0 extra clicks | Same product row increments qty (e.g. 1 -> 2), totals update | [ ] |
| 3 | Too much quantity | 1 click (or Enter) after setting qty above stock | Red error banner & status bar message, cart and stock unchanged | [ ] |
| 4 | Hold | 1 click (`Hold (F2)` or press `F2`) | No confirmation prompt; cart clears, order button appears in Held orders strip, status confirms hold | [ ] |
| 5 | Resume | 1 click on held-order button in strip | Non-empty current order is held automatically; clicked held order loads into cart immediately | [ ] |
| 6 | Pay cash exact | 2 actions (`Pay (F4)` / `F4` + `Enter`) | Payment dialog opens with Cash selected and exact payable prefilled; Enter confirms, Bill dialog opens, cart clears, stock reduced | [ ] |
| 7 | Cash with change | `Pay (F4)` + type received > payable + `Enter` | Live change shown (`received - payable`), Bill shows received and balance | [ ] |
| 8 | Open paid order within 7 days | `Open Order` + pick paid order (+ manager PIN if Cashier) | Opens editable copy with banner `Editing paid order <id>`, `Save Changes` and `Discard` shown | [ ] |
| 9 | Open paid order after 7 days | `Open Order` + pick paid order > 7 days old | Refused with clear core message; no edit starts | [ ] |
| 10 | Cashier special discount asks PIN | `Pay (F4)` -> `Special discount` -> enter manager PIN | Prompts `ApprovalDialog` for Cashier; valid PIN allows up to 8.00% discount | [ ] |
| 11 | Wrong PIN (Role / Approval) | Select Manager or trigger approval -> enter wrong PIN -> Confirm | Refused with red error message; role does not switch and restricted action is blocked | [ ] |
| 12 | Tab switch with camera running | Click another tab (e.g. Products) while camera runs on Orders | Active `CameraPanel` stops before leaving Orders; at most one camera runs at any time; switching role also stops camera | [ ] |
| 13 | Camera unplugged | Start app with no camera or unplug camera during use | Error message shown in banner and status bar; app stays open and manual barcode entry keeps working | [ ] |
