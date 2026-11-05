# Leviathan

Copyright (C) 2026 Shane O'Reilly

Leviathan is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General
Public License as published by the Free Software Foundation, version 3 (see [LICENSE](LICENSE)), together with the
additional terms below.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
details.

Source code: https://github.com/shaneoreilly/leviathan-amp-suite

## Additional terms (AGPLv3 section 7)

1. **Author attribution (section 7(b)).** Any copy or modified version of this program that displays Appropriate
   Legal Notices (for example an About, Credits or Settings screen) must keep in those notices the attribution
   "Leviathan by Shane O'Reilly" with a link to https://github.com/shaneoreilly/leviathan-amp-suite. Modified
   versions may add their own credits, but must not remove or obscure this one.
2. **Modified versions (section 7(c)).** Modified versions must be marked as changed from the original, and must
   not be presented as the original Leviathan or as made or endorsed by its author.

## Third-party software

- **NeuralAmpModelerCore** (both apps, NAM captures), MIT License, © Steven Atkinson.
  https://github.com/sdatkinson/NeuralAmpModelerCore — with **Eigen** (MPL-2.0, https://eigen.tuxfamily.org) and
  **nlohmann/json** (MIT, © Niels Lohmann). Fetched at build time, not stored in this repository.
- NAM capture files (`.nam`) are third-party content: they're not included and keep their creators' licenses.
- **JUCE** (Windows app, `windows/`), used under the GNU AGPLv3. © Raw Material Software Limited.
  https://github.com/juce-framework/JUCE
- **Steinberg ASIO SDK** (Windows app, ASIO support), used under the GNU GPLv3; downloaded at build time, not
  stored in this repository. ASIO is a trademark of Steinberg Media Technologies GmbH. https://www.steinberg.net/asiosdk
- Impulse-response packs (for example God's Cab) are third-party content, are not included, and keep their own
  licenses.
