ritual_au="build/local-au/RitualCodec_artefacts/Release/AU/Ritual Codec.component"

codesign --force --sign - --timestamp=none "$ritual_au"
codesign --verify --strict "$ritual_au"

ditto "$ritual_au" \
  "$HOME/Library/Audio/Plug-Ins/Components/Ritual Codec.component"

killall -u "$(id -un)" AudioComponentRegistrar || true
auval -v aufx RtCd Crrb
