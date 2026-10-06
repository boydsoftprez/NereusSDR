//=================================================================
// CATParser.cs
//=================================================================
// Copyright (C) 2012  Bob Tracy
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================


// Ported from Thetis Project Files/Source/Console/CAT/CATParser.cs
// Modification history (NereusSDR):
// 2026-10-04 - C++20/Qt6 parser/session dispatch by J.J. Boyd,
//              with AI-assisted transformation via Codex.

#include "CatParser.h"
#include <QRegularExpression>
namespace NereusSDR {
namespace {
// From Thetis CAT/CATParser.cs:1568-1607 [v2.10.3.15]. ProcessError codes.
constexpr int kBadCommandName = 1;
constexpr int kInactiveCommand = 2;
constexpr int kUnknownCommand = 3;
constexpr int kSuffixFormatError = 5;
constexpr int kSuffixLengthError = 6;
}
CatValidation CatParser::validate(const QByteArray& frame) const
{
    // From Thetis CAT/CATParser.cs:112-127,410-592 [v2.10.3.15].
    // Abort if the overall string length is less than 3 (aa;)
    // [original inline comment from CATParser.cs:117]
    //command length error
    // [original inline comment from CATParser.cs:121]
    // If there is no terminator, or the prefix or suffix
    // [original inline comment from CATParser.cs:413]
    // is invalid, abort.
    // [original inline comment from CATParser.cs:414]
    // If the command has a leading terminator(s) (like sent by WriteLog)
    // [original inline comment from CATParser.cs:416]
    // dump it and check the rest of the command.
    // [original inline comment from CATParser.cs:417]
    // If there is no terminator, or the prefix
    // [original inline comment from CATParser.cs:421]
    // is invalid, abort.
    // [original inline comment from CATParser.cs:422]
    // Now check to see if it's an extended command
    // [original inline comment from CATParser.cs:429]
    // Check the prefix
    // [original inline comment from CATParser.cs:435]
    // if no other errors are trapped, use this for default
    // [original inline comment from CATParser.cs:439]

    QByteArray command = frame;
    while (command.startsWith(';')) { command.remove(0, 1); }
    const auto failure = [](int code, const QByteArray& context) {
        return CatValidation{std::nullopt, "?;", code, context};
    };
    const qsizetype terminator = command.indexOf(';');
    if (command.size() < 3 || terminator < 2) {
        return failure(kBadCommandName, command);
    }
    const bool extended = command.left(2).toUpper() == "ZZ" && command.size() > 3;
    const qsizetype prefixWidth = extended ? 4 : 2;
    // Find the prefix in the xml document and get the parameter
    // [original inline comment from CATParser.cs:468]
    // values.
    // [original inline comment from CATParser.cs:469]

    // Extract the prefix from the command string
    // [original inline comment from CATParser.cs:461]
    const QByteArray code = command.left(prefixWidth).toUpper();
    const CatDescriptor* descriptor = m_catalog.find(code);
    if (!descriptor) { return failure(kUnknownCommand, command); }

    //					prefix = pfx;
    // [original inline comment from CATParser.cs:495]
    // If this is not an active command there is no use continuing.
    // [original inline comment from CATParser.cs:496]
    //inactive command
    // [original inline comment from CATParser.cs:513]
    //unknown command
    // [original inline comment from CATParser.cs:518]
    if (!descriptor->active) { return failure(kInactiveCommand, command); }
    if (terminator < prefixWidth) { return failure(kBadCommandName, command); }

    // Define the suffix as everything after the prefix and before
    // [original inline comment from CATParser.cs:541]
    // the first terminator.
    // [original inline comment from CATParser.cs:542]
    const QByteArray suffix = command.mid(prefixWidth, terminator - prefixWidth);
    // Normalize only the prefix; source text/EQ/GUID bytes retain case/spaces.
    if (descriptor->suffixKind == CatSuffixKind::Numeric) {
        // From Thetis CAT/CATParser.cs:550-571 [v2.10.3.15].
        static const QRegularExpression kNumericPattern(QStringLiteral("^[+-]?[Vv0-9]*$"));
    //modified 3/17/07 BT to correct bug in reading parameters with plus or minus sign
    // [original inline comment from CATParser.cs:560]
    // Check the suffix for illegal characters
    // [original inline comment from CATParser.cs:561]
    // [^0-9] = match any non-numeric character
    // [original inline comment from CATParser.cs:562]
    //Regex sfxpattern = new Regex("[^0-9]");
    // [original inline comment from CATParser.cs:563]
    //if(sfxpattern.IsMatch(sfx))
    // [original inline comment from CATParser.cs:564]
    //	return false;
    // [original inline comment from CATParser.cs:565]

    //illegal suffix format
    // [original inline comment from CATParser.cs:556]
        if (!kNumericPattern.match(QString::fromLatin1(suffix)).hasMatch()) {
            return failure(kSuffixFormatError, code);
        }
    }
    //suffix length error
    // [original inline comment from CATParser.cs:580]

    //bad suffix and but no errors trapped
    // [original inline comment from CATParser.cs:448]
    // Check the length against the struct requirements
    // [original inline comment from CATParser.cs:572]
    //suffix = sfx;
    // [original inline comment from CATParser.cs:575]
    const bool set = descriptor->setWidth >= 0 && suffix.size() == descriptor->setWidth;
    const bool get = descriptor->getWidth >= 0 && suffix.size() == descriptor->getWidth;
    if (!set && !get) { return failure(kSuffixLengthError, code + suffix); }
    const CatForm form = set && (!get || descriptor->formPrecedence == CatFormPrecedence::SetFirst)
        ? CatForm::Set : CatForm::Get;
    return {CatRequest{code, suffix, form}, {}, 0, {}};
}
QByteArray CatParser::formatError(const QByteArray& error, int code, const QByteArray& command,
                                const CatSessionContext& session) const
{
    // From Thetis CAT/CATParser.cs:1553-1611 [v2.10.3.15].
    if (!session.verboseErrors) { return error; }
    static constexpr const char* kErrorTexts[] = {
        "Undefined Error;", "Bad Command Name;", "Inactive Command;",
        "Unknown Command;", "Undefined Command Error;", "Suffix Format Error;",
        "Suffix Length Error;", "Feature Not Available;", "Form Must Be Open;",
        "Value out of bounds;", "Power must be on;"
    };
    const int index = code >= 0 && code < static_cast<int>(std::size(kErrorTexts)) ? code : 0;
    return "ZZEM:" + command + ':' + kErrorTexts[index];
}
QByteArray CatParser::formatValidationError(const CatValidation& validation,
                                          const CatSessionContext& session) const
{
    return formatError(validation.error, validation.verboseErrorCode, validation.errorCommand, session);
}
QByteArray CatParser::format(const CatDescriptor& descriptor, const CatRequest& request,
                           const CatCommandResult& result, const CatSessionContext& session) const
{
    if (result.kind == CatResultKind::Wire) { return result.data; }
    if (result.kind == CatResultKind::Silence) { return {}; }
    if (result.kind == CatResultKind::Error) {
        // From Thetis CAT/CATParser.cs:389-405,1535-1547 [v2.10.3.15].
        // Standard handler errors bypass ProcessError; extended errors use it.
        return request.code.startsWith("ZZ")
            ? formatError(result.data, result.verboseErrorCode, request.code + request.suffix, session) : result.data;
    }
    // From Thetis CAT/CATParser.cs:385-409,1535-1550 [v2.10.3.15].
    // if this is a standard command
    // [original inline comment from CATParser.cs:389]
    // and it's not an error
    // [original inline comment from CATParser.cs:391]
    // if it has the correct length
    // [original inline comment from CATParser.cs:394]
    // return the formatted CAT answer
    // [original inline comment from CATParser.cs:396]
    // no answer is required
    // [original inline comment from CATParser.cs:397]
    // processing incomplete for some reason
    // [original inline comment from CATParser.cs:400]
    // this was a bad command
    // [original inline comment from CATParser.cs:405]
    // Read successfully executed
    // [original inline comment from CATParser.cs:407]
    //rtncmd != Error1 && rtncmd != Error2 && rtncmd != Error3)
    // [original inline comment from CATParser.cs:1536]
    //Don't trim filter name string
    // [original inline comment from CATParser.cs:1540]
    // Fix in next generation.
    // [original inline comment from CATParser.cs:1541]

    QByteArray payload = result.data;
    if (payload.contains("?;")) {
        return request.code.startsWith("ZZ")
            ? formatError(payload, result.verboseErrorCode, request.code + request.suffix, session) : payload;
    }
    if (request.code.startsWith("ZZ")) {
        if (payload.size() == descriptor.answerWidth && descriptor.answerWidth > 0) {
            if (payload.startsWith(' ') && request.code != "ZZML" && request.code != "ZZMN") {
                payload = payload.trimmed();
            }
            return request.code + request.suffix + payload + ';';
        }
        return payload;
    }
    if (payload.size() == descriptor.answerWidth && descriptor.answerWidth > 0) {
        return request.code + payload + ';';
    }
    if (descriptor.answerWidth == -1 || payload.isEmpty()) { return {}; }
    return "O;";
}
} // namespace NereusSDR
