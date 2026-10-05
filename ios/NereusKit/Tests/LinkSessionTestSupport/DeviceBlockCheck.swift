// NereusSDR for iOS: checks a device sign-in block as the Core would, for the runners
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink

/// `"$device:signed"` from an app's runner (link document section 16.3): a
/// check, not a fill. The block the app sent must be five strings, with a
/// usable `shortName` when it has one, its `id` the fingerprint of its
/// `publicKey`, a canonical P-256 key, and its `signature` must verify over
/// this connection's transcript.
public enum DeviceBlockCheck {
    /// Why `block` fails, or nil when it passes.
    public static func failure(_ block: LinkJSON, challenge: Data, certificateSHA256: Data,
                               stationKey: Data) -> String? {
        guard case .object(let fields) = block else {
            return "the device block is not an object"
        }
        let required: Set<String> = ["id", "publicKey", "name", "kind", "signature"]
        let keys = Set(fields.keys)
        guard required.isSubset(of: keys), keys.subtracting(required).isSubset(of: ["shortName"]) else {
            return "the device block has keys \(keys.sorted()), not the five and an optional shortName"
        }
        guard case .string(let id)? = fields["id"], case .string(let keyText)? = fields["publicKey"],
              case .string(let name)? = fields["name"], case .string(let kind)? = fields["kind"],
              case .string(let signatureText)? = fields["signature"] else {
            return "the device block's five fields are not all strings"
        }
        if let shortName = fields["shortName"] {
            guard case .string(let text) = shortName,
                  DeviceName.isUsableShortName(text) else {
                return "the device block's shortName is not a usable short name"
            }
        }
        guard DeviceName.isUsable(name) else {
            return "the device block's name is not a usable name"
        }
        guard DeviceKeyAuthenticator.Kind(rawValue: kind) != nil else {
            return "the device block's kind \(kind) is not phone or tablet"
        }
        guard let key = Base64URL.decode(keyText), P256Wire.isCanonicalKey(key) else {
            return "the device block's publicKey is not a canonical P-256 key in base64url"
        }
        guard id == P256Wire.deviceId(spki: key) else {
            return "the device block's id is not the fingerprint of its publicKey"
        }
        guard let signature = Base64URL.decode(signatureText) else {
            return "the device block's signature is not base64url"
        }
        let transcript = DeviceKeyAuthenticator.transcript(challenge: challenge, certificateSHA256: certificateSHA256,
                                                           stationKey: stationKey, deviceKey: key)
        guard P256Wire.verify(signature: signature, over: transcript, spki: key) else {
            return "the device block's signature does not verify over this connection's transcript"
        }
        return nil
    }
}
