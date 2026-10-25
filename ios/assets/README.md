# Port-owned UI lettering

`settings-ko.png` is new lettering generated for this port at the owner's request.
Only the word 설정 is present on transparency; no original game plate or pixels
are included. The private Korean menu plates were style references and remain
outside the repository.

The native loader selects its main ink bounds (503,277)-(1271,634), ignores faint
alpha noise, and fits the mask through the existing plate and glow renderer.
`ios/tab_lettering_preview.py` runs that actual loader on the macOS CI runner.
Changing the source size or crop requires reviewing the plate-sized preview.
