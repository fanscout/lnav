/**
 * Copyright (c) 2013, Timothy Stack
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither the name of Timothy Stack nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file ansi_scrubber.cc
 */

#include "config.h"

#include <vector>

#include "pcrepp.hh"
#include "ansi_scrubber.hh"

using namespace std;

static pcrepp &ansi_regex(void)
{
    static pcrepp retval("\x1b\\[([\\d=;]*)([a-zA-Z])");

    return retval;
}

void scrub_ansi_string(std::string &str, string_attrs_t &sa)
{
    view_colors &vc = view_colors::singleton();
    vector<pair<string, string_attr_t> > attr_queue;
    pcre_context_static<60> context;
    vector<line_range> range_queue;
    pcrepp &regex = ansi_regex();
    pcre_input pi(str);

    while (regex.match(context, pi)) {
        pcre_context::capture_t *caps = context.all();
        struct line_range lr;
        bool has_attrs = false;
        int  attrs     = 0;
        int  bg = 0;
        int  fg = 0;
        int  lpc;

        switch (pi.get_substr_start(&caps[2])[0]) {
        case 'm':
            for (lpc = caps[1].c_begin;
                 lpc != (int)string::npos && lpc < caps[1].c_end; ) {
                int ansi_code = 0;

                if (sscanf(&(str[lpc]), "%d", &ansi_code) == 1) {
                    if (90 <= ansi_code && ansi_code <= 97) {
                        ansi_code -= 60;
                        attrs |= A_STANDOUT;
                    }
                    if (30 <= ansi_code && ansi_code <= 37) {
                        fg = ansi_code - 30;
                    }
                    if (40 <= ansi_code && ansi_code <= 47) {
                        bg = ansi_code - 40;
                    }
                    switch (ansi_code) {
                    case 1:
                        attrs |= A_BOLD;
                        break;

                    case 2:
                        attrs |= A_DIM;
                        break;

                    case 4:
                        attrs |= A_UNDERLINE;
                        break;

                    case 7:
                        attrs |= A_REVERSE;
                        break;
                    }
                }
                lpc = str.find(";", lpc);
                if (lpc != (int)string::npos) {
                    lpc += 1;
                }
            }
            if (fg != 0 || bg != 0) {
                attrs |= vc.ansi_color_pair(fg, bg);
            }
            has_attrs = true;
            break;

        case 'C':
            {
                int spaces = 0;

                if (sscanf(&(str[caps[1].c_begin]), "%d", &spaces) == 1 &&
                    spaces > 0) {
                    str.insert(caps[0].c_end, spaces, ' ');
                }
            }
            break;
        }
        str.erase(str.begin() + caps[0].c_begin,
                  str.begin() + caps[0].c_end);

        if (has_attrs) {
            if (!range_queue.empty()) {
                range_queue.back().lr_end = caps[0].c_begin;
            }
            lr.lr_start = caps[0].c_begin;
            lr.lr_end   = -1;
            range_queue.push_back(lr);
            attr_queue.push_back(make_string_attr("style", attrs));
        }

        pi.reset(str);
    }

    for (size_t lpc = 0; lpc < range_queue.size(); lpc++) {
        sa[range_queue[lpc]].insert(attr_queue[lpc]);
    }
}
