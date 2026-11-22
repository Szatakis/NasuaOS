#!/bin/sh
# install_payload.sh <payload_dir> <dest_dir>
#
# Installs every regular file found under <payload_dir> into <dest_dir>,
# preserving relative paths. Symbolic links are dereferenced and broken links
# are skipped, so optional payload data (for example model weights that are
# not checked into the repository) is installed only when present.
#
# Prints each installed path, relative to <dest_dir>, on stdout so callers
# can record exactly which files a payload owns.
set -eu

src=$1
dst=$2

[ -d "$src" ] || exit 0
[ -d "$dst" ] || { echo "install_payload.sh: destination '$dst' does not exist" >&2; exit 1; }

# Resolve both directories up front: the scan below changes the working
# directory, which would break relative destination paths.
case $src in /*) ;; *) src=$(CDPATH= cd -- "$src" && pwd);; esac
case $dst in /*) ;; *) dst=$(CDPATH= cd -- "$dst" && pwd);; esac

cd "$src"
find -L . -type f | while IFS= read -r f; do
	rel=${f#./}
	mkdir -p "$dst/$(dirname "$rel")"
	cp -L "$f" "$dst/$rel"
	printf '%s\n' "$rel"
done
