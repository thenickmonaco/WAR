(function() {
    var type_impls = Object.fromEntries([["alsa_sys",[]],["freetype",[]],["freetype_sys",[]],["harfbuzz_rs",[]],["harfbuzz_sys",[]],["nix",[]]]);
    if (window.register_type_impls) {
        window.register_type_impls(type_impls);
    } else {
        window.pending_type_impls = type_impls;
    }
})()
//{"start":55,"fragment_lengths":[15,16,20,19,20,11]}