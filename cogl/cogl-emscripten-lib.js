//"use strict";

var LibraryCoglEmscripten = {
    $CoglEmscripten__deps: ['$Browser', '$GL'],
    $CoglEmscripten: {

	windows: [],

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
};

autoAddDeps(LibraryCoglEmscripten, '$CoglEmscripten');
mergeInto(LibraryManager.library, LibraryCoglEmscripten);
