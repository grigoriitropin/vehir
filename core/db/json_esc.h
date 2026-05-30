// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#pragma once
#include <stddef.h>

static inline void json_esc(const char *s, char *out, size_t sz) {
    size_t o = 0;
    for (size_t i = 0; s[i] && o + 8 < sz; i++) {
        unsigned char c = (unsigned char)s[i];
        if      (c == '"')  { out[o++] = '\\'; out[o++] = '"';  }
        else if (c == '\\') { out[o++] = '\\'; out[o++] = '\\'; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n';  }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r';  }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't';  }
        else if (c < 0x20)  { out[o++] = (char)c; }
        else                { out[o++] = (char)c; }
    }
    out[o] = '\0';
}
