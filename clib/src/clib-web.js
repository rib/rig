//"use strict";

var LibraryClibWeb = {
    $ClibWeb__deps: ['$Browser'],
    $ClibWeb: {

	windows: [],
        images: [],
    },

    _c_web_console_assert: function (condition, message) {
        console.assert(condition, Pointer_stringify(message));
    },

    _c_web_console_trace: function () {
        console.trace();
    },
};

autoAddDeps(LibraryClibWeb, '$ClibWeb');
mergeInto(LibraryManager.library, LibraryClibWeb);
