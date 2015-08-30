//"use strict";

var LibraryClibWeb = {
    $ClibWeb__deps: ['$Browser'],
    $ClibWeb: {

	windows: [],
        images: [],
    },

    _c_web_console_log: function (message) {
        console.log(Pointer_stringify(message));
    },

    _c_web_console_info: function (message) {
        console.info(Pointer_stringify(message));
    },

    _c_web_console_warn: function (message) {
        console.warn(Pointer_stringify(message));
    },

    _c_web_console_error: function (message) {
        console.error(Pointer_stringify(message));
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
