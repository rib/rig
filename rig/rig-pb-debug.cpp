/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <config.h>

#include <clib.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/message.h>
C_BEGIN_DECLS
#define _Static_assert(X, Y)
#include <rig.pb-c.h>
#undef _Static_assert
C_END_DECLS

#include <rig.pb.h>

C_BEGIN_DECLS

void
rig_pb_print_ui(const Rig__UI *pb_ui)
{
    size_t len = protobuf_c_message_get_packed_size((ProtobufCMessage *)pb_ui);
    unsigned char *buf = (unsigned char *)c_malloc(len);
    protobuf_c_message_pack((ProtobufCMessage *)pb_ui, buf);

    rig::UI ui;
    if (!ui.ParseFromArray(buf, len)) {
        c_warning("Failed to parse UI for printing");
        return;
    }

    std::string ui_str;
    google::protobuf::TextFormat::PrintToString(ui, &ui_str);

    c_print(ui_str.c_str());

    c_free(buf);
}

void
rig_pb_print_frame_setup(const Rig__FrameSetup *pb_frame_setup)
{
    size_t len =
        protobuf_c_message_get_packed_size((ProtobufCMessage *)pb_frame_setup);
    unsigned char *buf = (unsigned char *)c_malloc(len);
    protobuf_c_message_pack((ProtobufCMessage *)pb_frame_setup, buf);

    rig::FrameSetup frame_setup;
    if (!frame_setup.ParseFromArray(buf, len)) {
        c_warning("Failed to parse FrameSetup for printing");
        return;
    }

    std::string ui_str;
    google::protobuf::TextFormat::PrintToString(frame_setup, &ui_str);

    c_print(ui_str.c_str());

    c_free(buf);
}

void
rig_pb_print_ui_diff(const Rig__UIDiff *pb_ui_diff)
{
    size_t len =
        protobuf_c_message_get_packed_size((ProtobufCMessage *)pb_ui_diff);
    unsigned char *buf = (unsigned char *)c_malloc(len);
    protobuf_c_message_pack((ProtobufCMessage *)pb_ui_diff, buf);

    rig::UIDiff ui_diff;
    if (!ui_diff.ParseFromArray(buf, len)) {
        c_warning("Failed to parse FrameSetup for printing");
        return;
    }

    std::string ui_str;
    google::protobuf::TextFormat::PrintToString(ui_diff, &ui_str);

    c_print(ui_str.c_str());

    c_free(buf);
}

C_END_DECLS
