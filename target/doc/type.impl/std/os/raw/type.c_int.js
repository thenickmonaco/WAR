(function() {
    var type_impls = Object.fromEntries([["freetype",[]],["glfw",[]],["harfbuzz_sys",[]],["portaudio_sys",[]]]);
    if (window.register_type_impls) {
        window.register_type_impls(type_impls);
    } else {
        window.pending_type_impls = type_impls;
    }
})()
//{"start":55,"fragment_lengths":[15,12,20,21]}