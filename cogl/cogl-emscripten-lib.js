//"use strict";

var LibraryCoglEmscripten = {
    $CoglEmscripten__deps: ['$Browser', '$GL'],
    $CoglEmscripten: {

        counter: 1, //To use as unique handles to pass to native code

        handles: {},

	windows: {},

        // XXX: This trick for keeping the table dense was nabbed from
        // library_gl.js:
        //
        // Get a new ID for images, while keeping the table dense and
        // fast. Creation is farely rare so it is worth optimizing
        // lookups later.
        //
        // I'm not a js expert but to me this looks like because it
        // never scans for null entries before incrementing counter
        // then this table will just continuously grow as IDs are
        // created and destroyed?
        getNewId: function (table) {
            var ret = CoglEmscripten.counter++;
            for (var i = table.length; i < ret; i++) {
                table[i] = null;
            }
            return ret;
        },

        touchX: 0, touchY: 0,
        savedKeydown: null,

    },

    Cogl_Emscripten_CreateWindow: function (width, height, x, y) {
      var canvas = document.createElement('canvas');
      canvas.width = width;
      canvas.height = height;
      canvas.style.left = x + 'px';
      canvas.style.top = y + 'px';
      document.body.appendChild(canvas);
      var id = CoglEmscripten.getNewId (CoglEmscripten.handles);
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
};

autoAddDeps(LibraryCoglEmscripten, '$CoglEmscripten');
mergeInto(LibraryManager.library, LibraryCoglEmscripten);
