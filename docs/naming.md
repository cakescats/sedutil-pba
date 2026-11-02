# Naming and discoverability

This document is meant to settle the repository name before the first public
release.

## How people search for this

People look for a fix to a concrete problem, and the tool name is the anchor:

- `sedutil`, `sedutil PBA`, `sedutil fork`, `sedutil not booting`,
  `sedutil hangs after password`, `sedutil sleep` / `S3`
- `OPAL 2.0`, `TCG OPAL`, `self-encrypting drive`, `SED`, `hardware encryption SSD`
- `PSID revert` (people who locked themselves out), `shadow MBR`, `pre-boot authentication`
- the brand of their drive plus `OPAL`: Samsung 990 PRO, Crucial, WD, Kingston

Searches go to the DTA wiki, issue sedutil#404, the Arch wiki
("Self-encrypting drives") and the AUR packages. Being linked from those places
matters more than the name.

## Candidates

| Name | Pros | Cons |
|---|---|---|
| `sedutil-pba` | Both main keywords in the URL. Plainly says what it is. | Plain. |
| `sedutil-ng` | Reads as "the successor". A common pattern. | Promises a full rewrite of the CLI as well. |
| `sedutil2` | Short. | Sounds like an official DTA version 2. Confusing next to DTA's own 1.49 versions. |
| `opal-pba` | Matches "OPAL". | Loses the `sedutil` keyword, which is the strongest one. |
| A brand name (e.g. `catlock`) | Memorable. Room to grow (UEFI PBA, Windows tools). | Nobody searches for it. Needs `sedutil` in the description anyway. |

## Recommendation

- **Repository: `cakescats/sedutil-pba`.** The product name in the UI and
  releases is "CakesCats PBA". When the UEFI PBA and the OS tools arrive, the
  repository can stay, because `sedutil` remains the search term.
- **Description:** "sedutil fork: pre-boot authorization (PBA) image for TCG
  OPAL 2.0 self-encrypting drives on current UEFI hardware. Several drives,
  SHA-512, reproducible builds."
- **Topics:** `sedutil` `opal` `tcg-opal` `self-encrypting-drive` `sed`
  `pba` `pre-boot-authentication` `nvme` `ssd` `encryption` `uefi` `buildroot` `psid`.
- **A GitHub fork of `Drive-Trust-Alliance/sedutil` as well.** Forks show up
  in the upstream network and "forks" list, where people look when a project
  is abandoned. GitHub search ranks forks below the parent, so the main
  repository should stay standalone. The fork's README points to it.
- **Links back:** a comment in sedutil#404, a note on the Arch wiki page and an
  AUR package (`cakes-pba-bin`). Each of these is a separate decision for the
  owner, because they post publicly.
