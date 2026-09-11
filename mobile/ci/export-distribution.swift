import Foundation
import Security
import CryptoKit
let query: [CFString:Any] = [kSecClass:kSecClassIdentity,kSecMatchLimit:kSecMatchLimitAll,kSecReturnRef:true]
var found: CFTypeRef?
guard SecItemCopyMatching(query as CFDictionary,&found)==errSecSuccess,let identities=found as? [SecIdentity] else { fatalError("Identity lookup failed") }
let wanted="CF531FDE8C2985AB5A3547FA481B8192DEBD5D02"
for identity in identities {
 var cert:SecCertificate?
 guard SecIdentityCopyCertificate(identity,&cert)==errSecSuccess,let cert=cert else {continue}
 let hash=Insecure.SHA1.hash(data:SecCertificateCopyData(cert) as Data).map {String(format:"%02X",$0)}.joined()
 if hash != wanted {continue}
 var parameters=SecItemImportExportKeyParameters();parameters.version=UInt32(SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION)
 let password=ProcessInfo.processInfo.environment["MAGLAVA_P12_PASSWORD"]! as NSString
 parameters.passphrase=Unmanaged.passUnretained(password)
 var output:CFData?
 let status=SecItemExport(identity,.formatPKCS12,[],&parameters,&output)
 guard status==errSecSuccess,let output=output else {print("Export failed: \(status)");exit(1)}
 let url=URL(fileURLWithPath:CommandLine.arguments[1]);try (output as Data).write(to:url)
 try FileManager.default.setAttributes([.posixPermissions:0o600],ofItemAtPath:url.path)
 print("Exported only the SPT Apple Distribution identity.");exit(0)
}
fatalError("Expected SPT distribution identity missing")
