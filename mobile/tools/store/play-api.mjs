// Closed testing only. Inspect by default; --upload explicitly commits a release.
import { readFile } from "node:fs/promises";
import { homedir } from "node:os";
import { sign } from "node:crypto";
export const packageName = "dev.fofo.maglava";
const credential = JSON.parse(
  await readFile(
    process.env.GOOGLE_APPLICATION_CREDENTIALS ||
      `${homedir()}/secrets/fofo-play-publisher.json`,
    "utf8",
  ),
);
const enc = (x) => Buffer.from(JSON.stringify(x)).toString("base64url");
const now = Math.floor(Date.now() / 1000);
const data = `${enc({ alg: "RS256", typ: "JWT" })}.${enc({ iss: credential.client_email, scope: "https://www.googleapis.com/auth/androidpublisher", aud: "https://oauth2.googleapis.com/token", iat: now, exp: now + 3600 })}`;
const assertion = `${data}.${sign("RSA-SHA256", Buffer.from(data), credential.private_key).toString("base64url")}`;
const auth = await fetch("https://oauth2.googleapis.com/token", {
  method: "POST",
  body: new URLSearchParams({
    grant_type: "urn:ietf:params:oauth:grant-type:jwt-bearer",
    assertion,
  }),
});
const token = await auth.json();
if (!auth.ok) throw new Error(`Google authentication failed (${auth.status})`);
export async function api(
  path,
  method = "GET",
  body,
  upload = false,
  mime = "application/octet-stream",
) {
  const response = await fetch(
    `https://androidpublisher.googleapis.com/${upload ? "upload/" : ""}androidpublisher/v3/applications/${packageName}/${path}`,
    {
      method,
      headers: {
        Authorization: `Bearer ${token.access_token}`,
        ...(body
          ? {
              "Content-Type": upload ? mime : "application/json",
            }
          : {}),
      },
      ...(body ? { body: upload ? body : JSON.stringify(body) } : {}),
    },
  );
  if (response.status === 204) return null;
  const value = await response.json();
  if (!response.ok)
    throw new Error(
      value.error?.message || `Google Play returned ${response.status}`,
    );
  return value;
}
