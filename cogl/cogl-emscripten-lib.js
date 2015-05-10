//"use strict";

var LibraryCoglEmscripten = {
    $CoglEmscripten__deps: ['$Browser', '$GL'],
    $CoglEmscripten: {

	windows: [],
        images: [],
    },

    Cogl_Emscripten_CreateWindow: function (width, height, x, y) {
        var canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        canvas.style.left = x + 'px';
        canvas.style.top = y + 'px';
        document.body.appendChild(canvas);
        var id = CoglEmscripten.windows.length;
        canvas.setAttribute("id", "cogl_window_" + id);
        canvas.setAttribute("class", "cogl_window");
        CoglEmscripten.windows[id] = canvas;
        return id;
    },

    Cogl_Emscripten_ShowWindow: function (canvas_id) {
        var canvas = CoglEmscripten.windows[canvas_id];
        canvas.style.visibility = "visible";
    },

    Cogl_Emscripten_HideWindow: function (canvas_id) {
        var canvas = CoglEmscripten.windows[canvas_id];
        canvas.style.visibility = "hidden";
    },

    Cogl_Emscripten_SetWindowSize: function (canvas_id, width, height) {
        var canvas = CoglEmscripten.windows[canvas_id];
        canvas.width = width;
        canvas.height = height;
    },

    Cogl_Emscripten_DestroyWindow: function (canvas_id) {
        CoglEmscripten.windows[canvas_id] = null;
    },

    Cogl_Emscripten_GetProcAddress__deps: ['emscripten_GetProcAddress'],
    Cogl_Emscripten_GetProcAddress: function(name_) {
        return _emscripten_GetProcAddress(name_);
    },

    _cg_webgl_image_create: function (url, onload, onerror, user_data) {
        var id = CoglEmscripten.images.length;
        var img = new Image();
        img.src = Pointer_stringify(url);
        img.onload = function() { if (onload) Runtime.dynCall('vii', onload, [id, user_data]); }
        img.onerror = function() { if (onerror) Runtime.dynCall('vii', onerror, [id, user_data]); }
        CoglEmscripten.images[id] = img;
        return id;
    },

    _cg_webgl_image_destroy: function (id) {
        var img = CoglEmscripten.images[id];

        //Make sure the browser forgets about our registered callback
        img.onload = null;

        //As an innocent web newbie I had also wondered about
        //cancelling any in-flight image loading in case we happen to
        //destroying and image before it was loaded but quickly got
        //scared by all kinds of black magic that's required to do
        //that in a way that's portable accross browsers, so I'll
        //ignore that for now :-)

        CoglEmscripten.images[id] = null;
    },
    _cg_webgl_image_get_width: function (id) { return CoglEmscripten.images[id].width; },
    _cg_webgl_image_get_height: function (id) { return CoglEmscripten.images[id].height; },

    _cg_webgl_tex_image_2d_with_image__sig: 'viiiiii',
    _cg_webgl_tex_image_2d_with_image: function(target,
                                                level,
                                                internal_format,
                                                format,
                                                type,
                                                image_handle,
                                                error_string)
    {
        var img = CoglEmscripten.images[image_handle];
        try {
            Module.ctx.texImage2D(target, level, internal_format, format, type, img);
        } catch (err) {
            return allocate(intArrayFromString(err.message), 'i8', ALLOC_NORMAL);
        }
    },

};

autoAddDeps(LibraryCoglEmscripten, '$CoglEmscripten');
mergeInto(LibraryManager.library, LibraryCoglEmscripten);
