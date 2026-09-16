#!/bin/sh
# whereami - report where the current session is located
#
# Implements bug#8500 (util: where am i)
# Prints: hostname, IP(s), cwd, user, OS, kernel, uptime, date, tty,
#         terminal, and a few other bits useful for identifying location.
#
# Usage:  whereami [-s|--short] [-h|--help]
#         where am i     (see shell snippet at bottom)

set -u

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
SHORT=0
for arg in "$@"; do
    case "$arg" in
        -s|--short) SHORT=1 ;;
        -h|--help)
            cat <<EOF
Usage: whereami [-s|--short]

Reports where the current session is located:
  - hostname (FQDN and short)
  - IP address(es)
  - current working directory
  - user / login
  - OS / kernel / architecture
  - uptime / load
  - date / time zone
  - tty / terminal / shell
  - gateway / DNS (best effort)
EOF
            exit 0
            ;;
        *) ;;
    esac
done

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
have() { command -v "$1" >/dev/null 2>&1; }

# Print "label: value" aligned, unless short mode
emit() {
    label=$1; shift
    value=$*
    [ -z "$value" ] && return 0
    if [ "$SHORT" -eq 1 ]; then
        printf '%s\n' "$value"
    else
        printf '%-14s %s\n' "$label:" "$value"
    fi
}

# Get primary IPv4 address (first non-loopback)
get_ip() {
    ip=""
    if have ip; then
        ip=$(ip -4 route get 1.1.1.1 2>/dev/null \
             | awk '{for(i=1;i<=NF;i++) if($i=="src"){print $(i+1); exit}}')
        [ -z "$ip" ] && ip=$(ip -4 addr show scope global 2>/dev/null \
             | awk '/inet /{sub(/\/.*/,"",$2); print $2; exit}')
    fi
    if [ -z "$ip" ] && have ifconfig; then
        ip=$(ifconfig 2>/dev/null \
             | awk '/inet /{print $2}' \
             | grep -v '^127\.' | head -n1 | sed 's/addr://')
    fi
    if [ -z "$ip" ] && have hostname; then
        ip=$(hostname -I 2>/dev/null | awk '{print $1}')
    fi
    printf '%s' "$ip"
}

# All non-loopback IPv4 addresses
get_all_ips() {
    if have ip; then
        ip -4 addr show scope global 2>/dev/null \
            | awk '/inet /{sub(/\/.*/,"",$2); print $2}' | paste -sd', ' -
    elif have ifconfig; then
        ifconfig 2>/dev/null \
            | awk '/inet /{print $2}' \
            | grep -v '^127\.' | sed 's/addr://' | paste -sd', ' -
    fi
}

# Default gateway
get_gateway() {
    if have ip; then
        ip route show default 2>/dev/null | awk '/default/{print $3; exit}'
    elif have route; then
        route -n 2>/dev/null | awk '/^0\.0\.0\.0/{print $2; exit}'
    elif have netstat; then
        netstat -rn 2>/dev/null | awk '/^0\.0\.0\.0|^default/{print $2; exit}'
    fi
}

# DNS servers
get_dns() {
    if [ -r /etc/resolv.conf ]; then
        awk '/^nameserver/{print $2}' /etc/resolv.conf | paste -sd', ' -
    fi
}

# ---------------------------------------------------------------------------
# Gather
# ---------------------------------------------------------------------------
HOST_SHORT=$(hostname -s 2>/dev/null || hostname 2>/dev/null)
HOST_FQDN=$(hostname -f 2>/dev/null || hostname 2>/dev/null)
IP_PRIMARY=$(get_ip)
IP_ALL=$(get_all_ips)
CWD=$(pwd 2>/dev/null)
USER_NAME=${USER:-$(id -un 2>/dev/null)}
UID_NUM=$(id -u 2>/dev/null)
TTY=$(tty 2>/dev/null)
TERM_NAME=${TERM:-}
SHELL_NAME=${SHELL:-}
PID_SHELL=$$
OS_NAME=$(uname -s 2>/dev/null)
OS_REL=$(uname -r 2>/dev/null)
ARCH=$(uname -m 2>/dev/null)
if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    OS_PRETTY=${PRETTY_NAME:-$OS_NAME}
else
    OS_PRETTY=$OS_NAME
fi
UPTIME_STR=""
if [ -r /proc/uptime ]; then
    UPTIME_STR=$(awk '{s=int($1); d=int(s/86400); h=int((s%86400)/3600); m=int((s%3600)/60);
                        printf "%dd %dh %dm", d, h, m}' /proc/uptime)
elif have uptime; then
    UPTIME_STR=$(uptime | sed 's/.*up //; s/, *[0-9]* user.*//')
fi
LOAD_AVG=""
if [ -r /proc/loadavg ]; then
    LOAD_AVG=$(cut -d' ' -f1-3 /proc/loadavg)
fi
DATE_NOW=$(date 2>/dev/null)
TZ_NAME=$(date +%Z 2>/dev/null)
GATEWAY=$(get_gateway)
DNS=$(get_dns)

# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
if [ "$SHORT" -eq 1 ]; then
    printf '%s\n' "$HOST_SHORT"
    [ -n "$IP_PRIMARY" ] && printf '%s\n' "$IP_PRIMARY"
    printf '%s\n' "$CWD"
    exit 0
fi

printf 'Where am I?\n'
printf '============\n'
emit "Hostname"   "$HOST_SHORT"
[ "$HOST_FQDN" != "$HOST_SHORT" ] && emit "FQDN" "$HOST_FQDN"
emit "IP"         "$IP_PRIMARY"
[ -n "$IP_ALL" ] && [ "$IP_ALL" != "$IP_PRIMARY" ] && emit "All IPs" "$IP_ALL"
emit "Directory"  "$CWD"
emit "User"       "$USER_NAME (uid=$UID_NUM)"
emit "Shell"      "$SHELL_NAME (pid $PID_SHELL)"
emit "TTY"        "$TTY"
emit "Terminal"   "$TERM_NAME"
emit "OS"         "$OS_PRETTY"
emit "Kernel"     "$OS_REL"
emit "Arch"       "$ARCH"
emit "Uptime"     "$UPTIME_STR"
emit "Load"       "$LOAD_AVG"
emit "Date"       "$DATE_NOW"
emit "Time zone"  "$TZ_NAME"
emit "Gateway"    "$GATEWAY"
emit "DNS"        "$DNS"