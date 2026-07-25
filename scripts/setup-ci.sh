#!/usr/bin/env bash
#
# setup-ci.sh — one-time setup of the `prod` release pipeline on GitHub.
#
#   ./scripts/setup-ci.sh                 # everything
#   ./scripts/setup-ci.sh --branch        # just create + protect prod
#   ./scripts/setup-ci.sh --apple         # signing certificate + notarisation
#   ./scripts/setup-ci.sh --notary        # just the notarisation credential
#   ./scripts/setup-ci.sh --azure         # just the Azure OIDC variables
#   ./scripts/setup-ci.sh --show          # what is configured right now
#
# Everything here is idempotent — safe to re-run.
#
# The Apple half needs a Developer ID Application certificate exported as a .p12.
# That export cannot be automated — macOS will not release a private key without
# a GUI prompt, verified — so the script tells you exactly what to click and then
# takes the file. Nothing is written to disk that is not cleaned up.
#
# Notarisation is fully automatic: --notary finds an App Store Connect API key in
# the keychain (service 'asc-mcp') or on disk and sets it as three secrets. An
# API key cannot sign code, only notarise, so the certificate is still required.
#
set -euo pipefail

REPO="${REPO:-SweetPapa/FloppyJam}"
DO_BRANCH=0 DO_APPLE=0 DO_AZURE=0 DO_SHOW=0 DO_NOTARY=0

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
        --show)   DO_SHOW=1;   shift ;;
        --repo)   REPO="${2:?}"; shift 2 ;;
        -h|--help) sed -n '2,16p' "$0" | sed 's/^#//;s/^ //'; exit 0 ;;
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
    info "Apple signing secrets"

    IDENT=$(security find-identity -v -p codesigning 2>/dev/null \
            | grep -F "Developer ID Application" | head -1 || true)
    [ -n "$IDENT" ] || die "no Developer ID Application certificate in the keychain"
    NAME=$(printf '%s' "$IDENT" | sed 's/.*"\(.*\)".*/\1/')
    TEAM=$(printf '%s' "$NAME" | sed -n 's/.*(\([A-Z0-9]\{10\}\))$/\1/p')
    echo "    identity: $NAME"
    echo "    team:     $TEAM"

    P12="$(mktemp -d)/cert.p12"
    trap 'rm -rf "$(dirname "$P12")"' EXIT
    cat <<EOF

    The private key cannot be exported without a GUI prompt, so:

      1. Keychain Access is about to open on "My Certificates".
      2. Right-click  $NAME
      3. Export...  ->  Personal Information Exchange (.p12)
      4. Save it to:  $P12
      5. Give it a password and remember it — you will paste it below.

EOF
    ask "press RETURN to open Keychain Access, or Ctrl-C to abort"; read -r _
    open -a "Keychain Access" 2>/dev/null || warn "could not open Keychain Access; export it manually"

    while [ ! -s "$P12" ]; do
        ask "waiting for $P12 — press RETURN once it is saved"; read -r _
    done

    ask "the password you exported the .p12 with:"
    read -rs P12PASS; echo
    [ -n "$P12PASS" ] || die "an empty .p12 password will not import in CI"
    # fail here rather than in a release
    openssl pkcs12 -in "$P12" -passin pass:"$P12PASS" -noout 2>/dev/null \
        || die "that password does not open the .p12"

    base64 -i "$P12" | gh secret set APPLE_CERT_P12            -R "$REPO"
    printf '%s' "$P12PASS" | gh secret set APPLE_CERT_PASSWORD  -R "$REPO"
    printf '%s' "$TEAM"    | gh secret set APPLE_TEAM_ID        -R "$REPO"
    echo "    certificate secrets set"
    rm -rf "$(dirname "$P12")"; trap - EXIT
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
