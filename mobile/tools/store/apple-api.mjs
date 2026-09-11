import { readFile } from "node:fs/promises";
import { homedir } from "node:os";
import { sign } from "node:crypto";
export const bundleId = "dev.fofo.maglava";
export const teamId = "6Y5SZ2K5XY";
export const keyId = process.env.ASC_KEY_ID || "4648S8AZQV";
export const issuerId =
  process.env.ASC_ISSUER_ID || "08a7930e-9034-41cd-ab35-41afa2b19813";
const key = await readFile(
  process.env.ASC_KEY_PATH || `${homedir()}/Downloads/AuthKey_${keyId}.p8`,
);
const enc = (value) => Buffer.from(JSON.stringify(value)).toString("base64url");
export async function apple(path, method = "GET", body) {
  const now = Math.floor(Date.now() / 1000);
  const payload = `${enc({ alg: "ES256", kid: keyId, typ: "JWT" })}.${enc({ iss: issuerId, iat: now, exp: now + 600, aud: "appstoreconnect-v1" })}`;
  const token = `${payload}.${sign("sha256", Buffer.from(payload), { key, dsaEncoding: "ieee-p1363" }).toString("base64url")}`;
  const response = await fetch(
    `https://api.appstoreconnect.apple.com/v1/${path}`,
    {
      method,
      headers: {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/json",
      },
      ...(body ? { body: JSON.stringify(body) } : {}),
    },
  );
  if (response.status === 204) return null;
  const result = await response.json();
  if (!response.ok) throw new Error(JSON.stringify(result.errors));
  return result;
}
