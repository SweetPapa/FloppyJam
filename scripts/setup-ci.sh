#!/usr/bin/env bash
#
# setup-ci.sh — one-time setup of the `prod` release pipeline on GitHub.
#
#   ./scripts/setup-ci.sh                 # everything
#   ./scripts/setup-ci.sh --branch        # just create + protect prod
#   ./scripts/setup-ci.sh --apple         # signing certificate + notarisation
#   ./scripts/setup-ci.sh --apple --identity "Forrester Terry"
#   ./scripts/setup-ci.sh --apple --cert ~/spt-sign.p12
#   ./scripts/setup-ci.sh --notary        # just the notarisation credential
#   ./scripts/setup-ci.sh --azure         # just the Azure OIDC variables
#   ./scripts/setup-ci.sh --show          # what is configured right now
#
# Everything here is idempotent — safe to re-run.
#
# The Apple half needs a Developer ID Application certificate exported as a .p12.
# That export cannot be automated — macOS will not release a private key without
# a GUI prompt — so there are three ways in, in order of preference:
#
#   1. Point .env at one you already exported. Non-interactive and repeatable:
#        CERT_LOCATION=/path/to/your.p12
#        CERT_PASS=the-password
#   2. --cert /path/to.p12
#   3. Let the script walk you through the export. If the keychain holds more
#      than one Developer ID it ASKS which one rather than taking the first;
#      --identity <substring> answers that up front.
#
# Notarisation is fully automatic: --notary finds an App Store Connect API key in
# the keychain (service 'asc-mcp') or on disk and sets it as three secrets. An
# API key cannot sign code, only notarise, so the certificate is still required.
#
set -euo pipefail

REPO="${REPO:-SweetPapa/FloppyJam}"
DO_BRANCH=0 DO_APPLE=0 DO_AZURE=0 DO_SHOW=0 DO_NOTARY=0
IDENTITY_WANT=""            # substring of the Developer ID to use
CERT_P12_ARG=""             # path to an already-exported .p12

# The .env lives next to the repo root, is gitignored, and is the intended way
# to make --apple non-interactive:  CERT_LOCATION=... / CERT_PASS=...
ENV_FILE="${ENV_FILE:-$(cd "$(dirname "$0")/.." && pwd)/.env}"

die()  { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
info() { printf '\033[36m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33mwarn:\033[0m %s\n' "$*" >&2; }
ask()  { printf '\033[35m ? \033[0m%s ' "$*"; }

if [ $# -eq 0 ]; then DO_BRANCH=1; DO_APPLE=1; DO_AZURE=1; fi
while [ $# -gt 0 ]; do
    case "$1" in
        --branch) DO_BRANCH=1; shift ;;
        --apple)  DO_APPLE=1;  shift ;;
        --azure)  DO_AZURE=1;  shift ;;
        --notary) DO_NOTARY=1; shift ;;
        --identity) IDENTITY_WANT="${2:?--identity needs a value}"; shift 2 ;;
        --cert)     CERT_P12_ARG="${2:?--cert needs a path}"; shift 2 ;;
        --env)      ENV_FILE="${2:?--env needs a path}"; shift 2 ;;
        --show)   DO_SHOW=1;   shift ;;
        --repo)   REPO="${2:?}"; shift 2 ;;
        -h|--help) sed -n '2,30p' "$0" | sed 's/^#//;s/^ //'; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

command -v gh >/dev/null || die "the GitHub CLI is not installed — https://cli.github.com"
gh auth status >/dev/null 2>&1 || die "not logged in — run: gh auth login"

# ------------------------------------------------------------------
# Notarisation credentials. An App Store Connect API key is preferred: there is
# no password to rotate, it can be revoked on its own without touching the
# signing certificate, and the same three values work locally and in CI.
ASC_KEY_ID_DEFAULT="${ASC_KEY_ID:-45S6SMU5GY}"
ASC_ISSUER_DEFAULT="${ASC_ISSUER_ID:-08a7930e-9034-41cd-ab35-41afa2b19813}"
ASC_KEY_SERVICE="${ASC_KEY_SERVICE:-asc-mcp}"

# Pull the .p8 out of the keychain. `security -w` returns hex whenever the blob
# is not clean UTF-8, and a PEM stored as raw data usually is not.
read_asc_key() {
    local raw
    raw="$(security find-generic-password -s "$ASC_KEY_SERVICE" -a "$1" -w 2>/dev/null || true)"
    [ -n "$raw" ] || return 1
    case "$raw" in
        *BEGIN*) printf '%s\n' "$raw" ;;
        *)       printf '%s' "$raw" | xxd -r -p 2>/dev/null ;;
    esac
}

set_notary_secrets() {
    local key="" kid="$ASC_KEY_ID_DEFAULT"
    info "notarisation credential"
    if key="$(read_asc_key "$kid")" && printf '%s' "$key" | grep -q "BEGIN PRIVATE KEY"; then
        echo "    App Store Connect API key $kid (from the '$ASC_KEY_SERVICE' keychain item)"
    else
        # fall back to a .p8 sitting on disk
        local found
        found="$(ls "$HOME"/code/*/config/AuthKey_*.p8 "$HOME"/Downloads/AuthKey_*.p8 \
                 "$HOME"/.appstoreconnect/private_keys/AuthKey_*.p8 2>/dev/null | head -1 || true)"
        if [ -n "$found" ]; then
            kid="$(basename "$found" | sed 's/AuthKey_\(.*\)\.p8/\1/')"
            key="$(cat "$found")"
            echo "    App Store Connect API key $kid (from $found)"
        fi
    fi

    if printf '%s' "$key" | grep -q "BEGIN PRIVATE KEY"; then
        printf '%s' "$key" | base64 | gh secret set ASC_KEY_P8   -R "$REPO"
        printf '%s' "$kid"                | gh secret set ASC_KEY_ID    -R "$REPO"
        printf '%s' "$ASC_ISSUER_DEFAULT" | gh secret set ASC_ISSUER_ID -R "$REPO"
        echo "    3 secrets set (ASC_KEY_P8, ASC_KEY_ID, ASC_ISSUER_ID)"
        return 0
    fi

    warn "no API key found; falling back to an Apple ID app-specific password"
    local appleid appass
    ask "Apple ID for notarisation [fterry24v2@gmail.com]:"
    read -r appleid; appleid="${appleid:-fterry24v2@gmail.com}"
    appass=$(security find-generic-password -s spt-notary -a "$appleid" -w 2>/dev/null || true)
    if [ -z "$appass" ]; then
        ask "app-specific password (appleid.apple.com -> App-Specific Passwords):"
        read -rs appass; echo
    fi
    [ -n "$appass" ] || { warn "no notarisation credential set"; return 1; }
    printf '%s' "$appleid" | gh secret set APPLE_ID           -R "$REPO"
    printf '%s' "$appass"  | gh secret set APPLE_APP_PASSWORD -R "$REPO"
    echo "    2 secrets set (APPLE_ID, APPLE_APP_PASSWORD)"
}

if [ "$DO_SHOW" = 1 ]; then
    info "secrets on $REPO"
    gh secret list -R "$REPO" 2>/dev/null | sed 's/^/    /' || echo "    (none)"
    info "variables on $REPO"
    gh variable list -R "$REPO" 2>/dev/null | sed 's/^/    /' || echo "    (none)"
    info "branches"
    gh api "repos/$REPO/branches" --jq '.[] | "    \(.name)\(if .protected then "  [protected]" else "" end)"'
    exit 0
fi

# ------------------------------------------------------------------
if [ "$DO_BRANCH" = 1 ]; then
    info "prod branch"
    DEFAULT=$(gh api "repos/$REPO" --jq .default_branch)
    if gh api "repos/$REPO/branches/prod" >/dev/null 2>&1; then
        echo "    already exists"
    else
        SHA=$(gh api "repos/$REPO/git/ref/heads/$DEFAULT" --jq .object.sha)
        gh api "repos/$REPO/git/refs" -f ref=refs/heads/prod -f sha="$SHA" >/dev/null
        echo "    created from $DEFAULT@${SHA:0:7}"
    fi

    info "branch protection on prod"
    # A release branch wants review and passing CI, and nothing wants force-push.
    # linear_history keeps `git log prod` readable as a list of what shipped.
    if gh api -X PUT "repos/$REPO/branches/prod/protection" \
        --input - >/dev/null 2>&1 <<'JSON'
{
  "required_status_checks": {
    "strict": true,
    "contexts": ["build + test (ubuntu-latest)", "build + test (macos-latest)"]
  },
  "enforce_admins": false,
  "required_pull_request_reviews": {
    "dismiss_stale_reviews": true,
    "require_code_owner_reviews": false,
    "required_approving_review_count": 1
  },
  "restrictions": null,
  "allow_force_pushes": false,
  "allow_deletions": false,
  "required_linear_history": true,
  "required_conversation_resolution": true
}
JSON
    then
        echo "    protected: PR + 1 review, CI must pass, no force push, linear history"
    else
        warn "could not set protection. On a free plan this needs a public repo;
      it also needs a token with 'repo' scope and admin on the repo."
    fi
fi

# ------------------------------------------------------------------
if [ "$DO_APPLE" = 1 ]; then
    info "Apple signing certificate"

    P12=""; P12PASS=""; OWN_TMP=""

    # --- 1. an already-exported .p12 named in .env ---------------------
    # By far the best path: you export once, put the path and password in a
    # gitignored .env, and this becomes non-interactive and repeatable. It also
    # sidesteps the guessing entirely — the certificate is whichever one you
    # chose to export, not whichever one the keychain happens to list first.
    if [ -f "$ENV_FILE" ]; then
        # shellcheck disable=SC1090
        set -a; . "$ENV_FILE"; set +a
        if [ -n "${CERT_LOCATION:-}" ]; then
            P12="${CERT_LOCATION/#\~/$HOME}"
            P12PASS="${CERT_PASS:-}"
            echo "    using the .p12 named in $ENV_FILE"
        fi
    fi

    # --- 2. or a path given on the command line ------------------------
    if [ -z "$P12" ] && [ -n "$CERT_P12_ARG" ]; then
        P12="${CERT_P12_ARG/#\~/$HOME}"
    fi

    # --- 3. or walk the export by hand --------------------------------
    if [ -z "$P12" ]; then
        # Pick the identity FIRST, and never silently. The old code took
        # `head -1` of the codesigning identities, which on a machine with both
        # a personal and a <redacted org> Developer ID hands you whichever sorts
        # first and tells you it is the answer.
        # A read loop rather than `mapfile`: macOS ships bash 3.2, where mapfile
        # does not exist, and every other script here has to run on it too.
        IDENTS=()
        while IFS= read -r line; do
            [ -n "$line" ] && IDENTS+=("$line")
        done < <(security find-identity -v -p codesigning 2>/dev/null \
                 | grep -F "Developer ID Application" || true)
        [ "${#IDENTS[@]}" -gt 0 ] || die "no Developer ID Application certificate in the keychain"

        CHOICE=""
        if [ -n "$IDENTITY_WANT" ]; then
            for line in "${IDENTS[@]}"; do
                case "$line" in *"$IDENTITY_WANT"*) CHOICE="$line"; break ;; esac
            done
            [ -n "$CHOICE" ] || die "no Developer ID matching '$IDENTITY_WANT'. available:
$(printf '  %s\n' "${IDENTS[@]}")"
        elif [ "${#IDENTS[@]}" -eq 1 ]; then
            CHOICE="${IDENTS[0]}"
        else
            echo "    more than one Developer ID Application certificate:"
            i=1
            for line in "${IDENTS[@]}"; do
                printf '      %d) %s\n' "$i" "$(printf '%s' "$line" | sed 's/.*"\(.*\)".*/\1/')"
                i=$((i + 1))
            done
            ask "which one? [1-${#IDENTS[@]}]"
            read -r PICK
            case "$PICK" in
                ''|*[!0-9]*) die "not a number: '$PICK'" ;;
            esac
            [ "$PICK" -ge 1 ] && [ "$PICK" -le "${#IDENTS[@]}" ] || die "out of range: $PICK"
            CHOICE="${IDENTS[$((PICK - 1))]}"
        fi

        NAME=$(printf '%s' "$CHOICE" | sed 's/.*"\(.*\)".*/\1/')
        echo "    identity: $NAME"

        OWN_TMP="$(mktemp -d)"
        P12="$OWN_TMP/cert.p12"
        trap 'rm -rf "$OWN_TMP"' EXIT
        cat <<EOF

    macOS will not release a private key without a GUI prompt, so:

      1. Keychain Access is about to open.
      2. Find  $NAME  under "My Certificates".
      3. Right-click -> Export... -> Personal Information Exchange (.p12)
      4. Save it to:  $P12
      5. Give it a password — you will paste it below.

    To skip this next time, export it once somewhere permanent and add to
    $ENV_FILE:

      CERT_LOCATION=/path/to/your.p12
      CERT_PASS=the-password

EOF
        ask "press RETURN to open Keychain Access, or Ctrl-C to abort"; read -r _
        open -a "Keychain Access" 2>/dev/null || warn "could not open Keychain Access; export it manually"
        while [ ! -s "$P12" ]; do
            ask "waiting for $P12 — press RETURN once it is saved"; read -r _
        done
    fi

    [ -f "$P12" ] || die "no .p12 at $P12"
    if [ -z "$P12PASS" ]; then
        ask "the password the .p12 was exported with:"
        read -rs P12PASS; echo
    fi
    [ -n "$P12PASS" ] || die "an empty .p12 password will not import in CI"

    # Validate with `security import` into a throwaway keychain, which is the
    # same call the release workflow makes. openssl is a poor proxy here:
    # OpenSSL 3 refuses the legacy cipher Keychain Access exports with unless
    # you pass -legacy, so it reports a perfectly good .p12 as a bad password.
    PROBE="$HOME/.setup-ci-probe.keychain-db"
    PROBE_PASS=$(openssl rand -hex 16)
    security delete-keychain "$PROBE" 2>/dev/null || true
    security create-keychain -p "$PROBE_PASS" "$PROBE"
    security unlock-keychain -p "$PROBE_PASS" "$PROBE"
    if security import "$P12" -k "$PROBE" -P "$P12PASS" -T /usr/bin/codesign >/dev/null 2>&1; then
        FOUND=$(security find-identity -v -p codesigning "$PROBE" \
                | grep -F "Developer ID Application" | head -1 || true)
        NAME=$(printf '%s' "$FOUND" | sed 's/.*"\(.*\)".*/\1/')
        # the team comes out of the certificate itself, not out of a guess
        TEAM=$(printf '%s' "$NAME" | sed -n 's/.*(\([A-Z0-9]\{10\}\))$/\1/p')
        security delete-keychain "$PROBE" 2>/dev/null || true
        [ -n "$NAME" ] || die "the .p12 imported but holds no Developer ID Application identity"
        echo "    contains: $NAME"
        echo "    team:     $TEAM"
    else
        security delete-keychain "$PROBE" 2>/dev/null || true
        die "could not import the .p12 — wrong password, or it has no private key"
    fi

    base64 -i "$P12" | gh secret set APPLE_CERT_P12            -R "$REPO"
    printf '%s' "$P12PASS" | gh secret set APPLE_CERT_PASSWORD  -R "$REPO"
    printf '%s' "$TEAM"    | gh secret set APPLE_TEAM_ID        -R "$REPO"
    echo "    3 secrets set (APPLE_CERT_P12, APPLE_CERT_PASSWORD, APPLE_TEAM_ID)"
    [ -n "$OWN_TMP" ] && { rm -rf "$OWN_TMP"; trap - EXIT; }
    set_notary_secrets
fi

# ------------------------------------------------------------------
if [ "$DO_NOTARY" = 1 ]; then set_notary_secrets; fi

# ------------------------------------------------------------------
if [ "$DO_AZURE" = 1 ]; then
    info "Azure Trusted Signing (OIDC)"
    command -v az >/dev/null || { warn "no Azure CLI; skipping"; exit 0; }
    ACC=$(az account show -o json 2>/dev/null || true)
    [ -n "$ACC" ] || { warn "not logged in to Azure (az login); skipping"; exit 0; }

    TENANT=$(printf '%s' "$ACC" | sed -n 's/.*"tenantId": *"\([^"]*\)".*/\1/p' | head -1)
    SUB=$(printf '%s' "$ACC" | sed -n 's/.*"id": *"\([^"]*\)".*/\1/p' | head -1)
    echo "    tenant:       $TENANT"
    echo "    subscription: $SUB"

    cat <<EOF

    OIDC means no client secret in GitHub, but it does need an app registration
    with a federated credential. If you have not made one yet:

      az ad app create --display-name floppyjam-ci
      APP_ID=\$(az ad app list --display-name floppyjam-ci --query '[0].appId' -o tsv)
      az ad sp create --id "\$APP_ID"
      az role assignment create --assignee "\$APP_ID" \\
        --role "Artifact Signing Certificate Profile Signer" \\
        --scope "/subscriptions/$SUB/resourceGroups/spt/providers/Microsoft.CodeSigning/codesigningaccounts/spt-cert"
      az ad app federated-credential create --id "\$APP_ID" --parameters '{
        "name": "github-prod",
        "issuer": "https://token.actions.githubusercontent.com",
        "subject": "repo:$REPO:ref:refs/heads/prod",
        "audiences": ["api://AzureADTokenExchange"]
      }'

EOF
    ask "the client (application) id of that app registration, or RETURN to skip:"
    read -r CLIENTID
    if [ -n "$CLIENTID" ]; then
        gh variable set AZURE_CLIENT_ID       -R "$REPO" -b "$CLIENTID"
        gh variable set AZURE_TENANT_ID       -R "$REPO" -b "$TENANT"
        gh variable set AZURE_SUBSCRIPTION_ID -R "$REPO" -b "$SUB"
        echo "    3 variables set"
    else
        warn "skipped — the Windows build will publish unsigned until these are set"
    fi
fi

info "done. Push to prod to cut a release:  git push origin main:prod"
