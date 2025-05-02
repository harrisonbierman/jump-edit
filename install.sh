#!/usr/bin/env bash
set -e

# 1) Install binary
install -Dm755 jump_edit /usr/local/bin/jump_edit

# 2) Install shell snippet system-wide
tee /etc/profile.d/jump_edit.sh >/dev/null << 'EOF'
#!/usr/bin/env bash
je() {

	if [[ $1 == add ||
		$1 == list ||
		$1 == --help ||
		$1 == rm ||
		$1 == default-editor ]]; then
		jump_edit "$@"
		return
	fi

	local script
	script="$(jump_edit "$@")" || return

	eval "$script"
}
EOF
chmod +x /etc/profile.d/jump_edit.sh

echo "jump_edit installed and /etc/profile.d/jump_edit.sh created."
echo "Start a new shell or run: source /etc/profile.d/jump_edit.sh"
