# 4Coder Finnerale

This can be cloned into the `custom` directory.

The only modification needed is copying the `4coder_terickson_language.h` into the `custom` directory.
This is neccesary when using custom languages such as Odin or Rust.

Custom language support uses Tyler's config:
https://github.com/terickson001/4files

The cursor is from Fleury:
https://github.com/ryanfleury/4coder_fleury

In `project.4coder` I use these commands to build it:
```
build_super_x64_win32 = "custom\\bin\\buildsuper_x64-win.bat custom\\4coder-finnerale\\4coder_finnerale.cpp";
build_super_x86_win32 = "custom\\bin\\buildsuper_x86-win.bat custom\\4coder-finnerale\\4coder_finnerale.cpp";
build_super_x64_linux  = "custom/bin/buildsuper_x64-linux.sh custom/4coder-finnerale/4coder_finnerale.cpp";
build_super_x86_linux  = "custom/bin/buildsuper_x86-linux.sh custom/4coder-finnerale/4coder_finnerale.cpp";
build_super_x64_mac  = "custom/bin/buildsuper_x64-mac.sh custom/4coder-finnerale/4coder_finnerale.cpp";
```

Languages can be build with this script:
```bash
#! /usr/bin/env bash

LANG=${1:-odin}

./custom/bin/build_one_time.sh ./custom/languages/${LANG}/lexer_gen.cpp
./one_time
rm ./one_time
```