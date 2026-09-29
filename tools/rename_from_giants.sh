# tools/rename_from_giants.sh <src> <dst>: copy a GiantRecomp portal file into this library.
set -eu
mkdir -p "$(dirname "$2")"
sed -e 's/giantsrecomp::portal/skylanders::portal/g' \
    -e 's/namespace giantsrecomp/namespace skylanders/g' \
    -e 's/giantsrecomp::/skylanders::/g' \
    -e 's/Giants Recompiled/the game/g' \
    -e 's/giantsrecomp_portal/skylanders_portal_core/g' \
    "$1" > "$2"
