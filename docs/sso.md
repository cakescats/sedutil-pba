# Corporate integration: SSO and MDM (PBA side)

A company configures the PBA centrally and connects it to its own systems:

- **SSO.** The company's sign-in (Entra ID, Okta, Google, Keycloak or any
  OpenID Connect provider) decides who may open a machine, instead of a
  helpdesk reading out passwords.
- **MDM.** The same tools that manage the fleet roll out the PBA and its
  configuration.

The PBA hands a request over to SSO through a QR code on F2. The phone
carries the sign-in, so the PBA needs no network and no clock.

The unlock server, MDM rollout scripts and the corporate docs live in
**[cakescats/sedutil-opal-enterprise](https://github.com/cakescats/sedutil-opal-enterprise)**.
This page covers the PBA side.

## On the machine

When the feature is configured, the password screen shows:

```
Forgot the password? F2 unlocks with your company account
```

F2 opens a screen with a QR code. Below it, for typing by hand, are the
server address, the machine id and a code like `AEBA-GBCJ-7V2Z-PR3S`. Any
key goes back to the password prompt.

The QR code holds `<sso_url>/m/<machine id>?c=<code>`. The code is a fresh
random challenge plus an HMAC-SHA512 of it with the machine key. A new code
is made every time F2 is pressed. The server accepts each code once, and only
from a user who is allowed to unlock that machine.

## Turning it on

The feature is off in released images. Turn it on with the
[image builder](builder.md):

```
python3 cakes-builder.py build -i cakes-pba-<version>-default.img.gz -o pc-0042.img \
    --sso-url https://unlock.example.com --machine-id PC-0042
```

The builder makes up a machine key and writes it to `pc-0042.img.machine.json`
for the unlock server. Settings set this way:

| Key | Meaning |
|---|---|
| `sso_url` | The unlock server. |
| `sso_machine_id` | This machine's id on the server. |
| `sso_key` | The machine key, at least 16 bytes in hex. Only read from `/etc/cakes-pba.conf`, never from the kernel command line. |

## What the machine key proves

The key sits in the PBA image, and the shadow MBR can be read without a
password. Whoever holds the drive can read the key and make valid codes for
it. A code only proves that the request comes from this machine's PBA or
from its drive. What protects the password is the sign-in: the server only
shows it to the users listed for the machine, and records every request.
Keeping the key in a TPM, so that it proves the machine itself, is on the
[roadmap](roadmap.md).
