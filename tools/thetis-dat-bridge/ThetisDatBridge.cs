// NereusSDR - Thetis container (.dat) envelope bridge.
//
// Thetis saves one meter container per .dat file as
//     Base64( GZip( BinaryFormatter.Serialize( Dictionary<string,string> ) ) )
// and each item group's settings as
//     "1|" + Base64( GZip( BinaryFormatter.Serialize(
//                 ConcurrentDictionary<string,object> ) ) )
// with '/' written as "[backslash]".
//
// Modes:
//   unpack   <input.dat>  <output.json>   container dictionary -> JSON
//   pack     <input.json> <output.dat>    JSON -> container dictionary
//   settings <input.txt>  <output.json>   one ItemGroupSettings_2 value -> JSON
//
// The semantic mapping to NereusSDR lives in
// src/gui/containers/ThetisContainerFile.cpp; this program only converts
// the .NET envelopes.
//
// Build (Windows .NET Framework 4.x):
//   csc.exe /nologo /target:exe /out:thetis-dat-bridge.exe ThetisDatBridge.cs

using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Formatters.Binary;
using System.Text;

internal static class ThetisDatBridge
{
    private static int Main(string[] args)
    {
        if (args.Length != 3)
        {
            Console.Error.WriteLine("usage: thetis-dat-bridge unpack|pack|settings <input> <output>");
            return 2;
        }
        try
        {
            if (string.Equals(args[0], "unpack", StringComparison.OrdinalIgnoreCase))
            {
                string base64 = File.ReadAllText(args[1], Encoding.ASCII).Trim();
                byte[] compressed = Convert.FromBase64String(base64);
                Dictionary<string, string> values = ReadDictionary(compressed);
                File.WriteAllText(args[2], WriteJson(values), new UTF8Encoding(false));
                return 0;
            }
            if (string.Equals(args[0], "pack", StringComparison.OrdinalIgnoreCase))
            {
                Dictionary<string, string> values = ReadJson(File.ReadAllText(args[1], Encoding.UTF8));
                byte[] compressed = WriteDictionary(values);
                File.WriteAllText(args[2], Convert.ToBase64String(compressed), Encoding.ASCII);
                return 0;
            }
            if (string.Equals(args[0], "settings", StringComparison.OrdinalIgnoreCase))
            {
                string text = File.ReadAllText(args[1], Encoding.UTF8).Trim();
                var settings = ReadSettings(text);
                File.WriteAllText(args[2], WriteValue(settings), new UTF8Encoding(false));
                return 0;
            }
            Console.Error.WriteLine("unknown mode: " + args[0]);
            return 2;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error.GetType().Name + ": " + error.Message);
            return 1;
        }
    }

    // ---- Dictionary<string,string> envelope --------------------------------

    private static Dictionary<string, string> ReadDictionary(byte[] compressed)
    {
        using (var memory = new MemoryStream(compressed))
        using (var gzip = new GZipStream(memory, CompressionMode.Decompress))
        {
            var formatter = new BinaryFormatter();
            object value = formatter.Deserialize(gzip);
            var dictionary = value as Dictionary<string, string>;
            if (dictionary == null) { throw new InvalidDataException("not a Dictionary<string,string>"); }
            return dictionary;
        }
    }

    private static byte[] WriteDictionary(Dictionary<string, string> values)
    {
        using (var memory = new MemoryStream())
        {
            using (var gzip = new GZipStream(memory, CompressionMode.Compress, true))
            {
                var formatter = new BinaryFormatter();
                formatter.Serialize(gzip, values);
            }
            return memory.ToArray();
        }
    }

    // ---- clsIGSettings._settings envelope ----------------------------------
    // ToString2 serializes only the ConcurrentDictionary<string,object>.
    // Version tag "1|" precedes it; '/' is written as "[backslash]".

    private static object ReadSettings(string text)
    {
        int bar = text.IndexOf('|');
        if (bar < 0 || text.Substring(0, bar) != "1") { throw new InvalidDataException("unsupported settings version"); }
        string payload = text.Substring(bar + 1).Replace("[backslash]", "/");
        byte[] compressed = Convert.FromBase64String(payload);
        using (var memory = new MemoryStream(compressed))
        using (var gzip = new GZipStream(memory, CompressionMode.Decompress))
        {
            var formatter = new BinaryFormatter { Binder = new ThetisBinder() };
            return formatter.Deserialize(gzip);
        }
    }

    private sealed class ThetisBinder : SerializationBinder
    {
        public override Type BindToType(string assemblyName, string typeName)
        {
            switch (typeName)
            {
                case "Thetis.MeterManager+Reading": return typeof(BridgeReading);
                case "Thetis.MeterManager+clsBarItem+BarStyle": return typeof(BridgeBarStyle);
                case "Thetis.MeterManager+clsBarItem+Units": return typeof(BridgeUnits);
                case "Thetis.OtherButtonMacroSettings": return typeof(BridgeOtherButtonMacroSettings);
                case "Thetis.OtherButtonMacroSettings+OB_ButtonState": return typeof(BridgeOtherButtonMacroSettings.OB_ButtonState);
                case "Thetis.MeterManager+clsButtonBox+IndicatorType": return typeof(BridgeIndicatorType);
            }
            Type resolved = typeName.IndexOf(',') >= 0 ? Type.GetType(typeName) : null;
            if (resolved == null && !string.IsNullOrEmpty(assemblyName))
            {
                resolved = Type.GetType(typeName + ", " + assemblyName);
            }
            if (resolved == null) { resolved = Type.GetType(typeName); }
            if (resolved == null)
            {
                throw new SerializationException("unmapped type: " + typeName + " (" + assemblyName + ")");
            }
            return resolved;
        }
    }

    // Thetis enum values, kept so a dump reads with the same names.
    private enum BridgeReading
    {
        NONE = -1, SIGNAL_STRENGTH = 0, AVG_SIGNAL_STRENGTH, ADC_PK, ADC_AV, AGC_PK, AGC_AV,
        AGC_GAIN, ESTIMATED_PBSNR, MIC, PWR, ALC, EQ, LEVELER, COMP, ALC_G, ALC_GROUP, LVL_G,
        MIC_PK, ALC_PK, EQ_PK, LEVELER_PK, COMP_PK, CFC_PK = 24, CFC_AV, CFC_G,
        REVERSE_PWR, SWR, DRIVE_FWD_ADC, FWD_ADC, REV_ADC, DRIVE_PWR, PA_FWD_PWR, PA_REV_PWR,
        CAL_FWD_PWR, REV_VOLT, FWD_VOLT, VOLTS, AMPS, AZ, ELE, VFOA_FREQ, VFOB_FREQ,
        VFOSUBA_FREQ, TX_FREQ, TIME_UTC, DATE_UTC, TIME_LOC, DATE_LOC, VFOA_BAND, VFOB_BAND,
        VFOSUBA_BAND, VFOA_FILTER_NAME, VFOB_FILTER_NAME, TX_BAND, VFOA_MODE, VFOB_MODE,
        SPLIT, RX2_ENABLED, VFOB_TX, SUB_RX, CUSTOM_PK, CUSTOM_AV, SIGNAL_MAX_BIN = 72,
        ADC_MAX_MAG = 73, LAST
    }
    private enum BridgeBarStyle { None = 0, Line, SolidFilled, GradientFilled, Segments }
    private enum BridgeUnits { DBM = 0, S_UNTS, U_V }
    private enum BridgeIndicatorType
    {
        RING = 0, BAR_LEFT, BAR_RIGHT, BAR_BOTTOM, BAR_TOP, DOT_LEFT, DOT_RIGHT,
        DOT_BOTTOM, DOT_TOP, DOT_TOP_LEFT, DOT_TOP_RIGHT, DOT_BOTTOM_LEFT,
        DOT_BOTTOM_RIGHT, TEXT_ICON_COLOUR, LAST = 99
    }

    // Thetis ucOtherButtonsOptionsGrid.OtherButtonMacroSettings. Fields are
    // kept verbatim so BinaryFormatter can populate them; the dump is stored
    // with the imported item even though NereusSDR's macro buttons differ.
    [Serializable]
#pragma warning disable 0649 // Populated by BinaryFormatter during the Thetis import.
    private sealed class BridgeOtherButtonMacroSettings
    {
        public enum OB_ButtonState { OFF = 0, ON, TOGGLE, LED, CONT_VIS, CAT }
        internal int _number;
        internal string _on_text;
        internal string _off_text;
        internal string _notes;
        internal bool _closes_parent;
        internal bool[] _closes_container = new bool[4];
        internal bool[] _opens_container = new bool[4];
        internal string[] _close_container_id = new string[4];
        internal string[] _open_container_id = new string[4];
        internal bool[] _open_uses_location = new bool[4];
        internal bool[] _send_via_mmio = new bool[4];
        internal string[] _mmio_4char = new string[4];
        internal string[] _mmio_message_on = new string[4];
        internal string[] _mmio_message_off = new string[4];
        internal OB_ButtonState _buttonstate_type;
        internal string _led_indicator_four_char;
        internal string _buttonstate_container_visible_id;
        internal string _buttonstate_cat_on_reply;
        internal bool _run_state_command_on_visible;
        internal bool[] _cat_macro_send = new bool[1];
        internal bool _on_state;
        internal string _cat_macro;
    }
#pragma warning restore 0649

    // ---- JSON --------------------------------------------------------------

    private static string WriteValue(object value)
    {
        var text = new StringBuilder();
        AppendValue(text, value);
        return text.ToString();
    }

    private static void AppendValue(StringBuilder text, object value)
    {
        if (value == null) { text.Append("null"); return; }
        string s = value as string;
        if (s != null) { AppendJsonString(text, s); return; }
        if (value is bool) { text.Append(((bool)value) ? "true" : "false"); return; }
        if (value is byte || value is sbyte || value is short || value is ushort
            || value is int || value is uint || value is long || value is ulong)
        {
            text.Append(Convert.ToInt64(value, CultureInfo.InvariantCulture).ToString(CultureInfo.InvariantCulture));
            return;
        }
        if (value is float) { text.Append(((float)value).ToString("R", CultureInfo.InvariantCulture)); return; }
        if (value is double) { text.Append(((double)value).ToString("R", CultureInfo.InvariantCulture)); return; }
        if (value is decimal) { text.Append(((decimal)value).ToString(CultureInfo.InvariantCulture)); return; }
        if (value is Color)
        {
            Color color = (Color)value;
            text.Append("\"#").Append(color.ToArgb().ToString("X8", CultureInfo.InvariantCulture)).Append('"');
            return;
        }
        if (value is Guid)
        {
            AppendJsonString(text, ((Guid)value).ToString());
            return;
        }
        if (value is Enum)
        {
            Enum e = (Enum)value;
            text.Append("{\"$enum\":");
            AppendJsonString(text, e.GetType().Name);
            text.Append(",\"name\":");
            AppendJsonString(text, e.ToString());
            text.Append(",\"value\":");
            text.Append(Convert.ToInt64(e, CultureInfo.InvariantCulture).ToString(CultureInfo.InvariantCulture));
            text.Append('}');
            return;
        }
        if (value is BridgeOtherButtonMacroSettings)
        {
            // Dump the macro definition itself, not just its type name, so
            // the container can show Thetis's macro captions and send the
            // same MMIO/CAT message the button sent in Thetis.
            BridgeOtherButtonMacroSettings macro = (BridgeOtherButtonMacroSettings)value;
            text.Append('{');
            AppendJsonString(text, "number"); text.Append(':');
            text.Append(macro._number.ToString(CultureInfo.InvariantCulture));
            text.Append(',');
            AppendJsonString(text, "on_text"); text.Append(':');
            AppendJsonString(text, macro._on_text ?? String.Empty);
            text.Append(',');
            AppendJsonString(text, "off_text"); text.Append(':');
            AppendJsonString(text, macro._off_text ?? String.Empty);
            text.Append(',');
            AppendJsonString(text, "notes"); text.Append(':');
            AppendJsonString(text, macro._notes ?? String.Empty);
            text.Append(',');
            AppendJsonString(text, "buttonstate_type"); text.Append(':');
            AppendJsonString(text, macro._buttonstate_type.ToString());
            text.Append(',');
            AppendJsonString(text, "on_state"); text.Append(':');
            text.Append(macro._on_state ? "true" : "false");
            text.Append(',');
            AppendJsonString(text, "cat_macro"); text.Append(':');
            AppendJsonString(text, macro._cat_macro ?? String.Empty);
            text.Append(',');
            AppendJsonString(text, "cat_macro_send"); text.Append(':');
            AppendValue(text, macro._cat_macro_send);
            text.Append(',');
            AppendJsonString(text, "buttonstate_container_visible_id"); text.Append(':');
            AppendJsonString(text, macro._buttonstate_container_visible_id ?? String.Empty);
            text.Append(',');
            AppendJsonString(text, "send_via_mmio"); text.Append(':');
            AppendValue(text, macro._send_via_mmio);
            text.Append(',');
            AppendJsonString(text, "mmio_4char"); text.Append(':');
            AppendValue(text, macro._mmio_4char);
            text.Append(',');
            AppendJsonString(text, "mmio_message_on"); text.Append(':');
            AppendValue(text, macro._mmio_message_on);
            text.Append(',');
            AppendJsonString(text, "mmio_message_off"); text.Append(':');
            AppendValue(text, macro._mmio_message_off);
            text.Append('}');
            return;
        }
        if (value is IDictionary)
        {
            IDictionary dictionary = (IDictionary)value;
            text.Append('{');
            bool first = true;
            foreach (DictionaryEntry entry in dictionary)
            {
                if (!first) { text.Append(','); }
                first = false;
                AppendJsonString(text, Convert.ToString(entry.Key, CultureInfo.InvariantCulture));
                text.Append(':');
                AppendValue(text, entry.Value);
            }
            text.Append('}');
            return;
        }
        if (value is IEnumerable)
        {
            text.Append('[');
            bool first = true;
            foreach (object item in (IEnumerable)value)
            {
                if (!first) { text.Append(','); }
                first = false;
                AppendValue(text, item);
            }
            text.Append(']');
            return;
        }
        AppendJsonString(text, Convert.ToString(value, CultureInfo.InvariantCulture));
    }

    private static string WriteJson(Dictionary<string, string> values)
    {
        var sorted = new List<string>(values.Keys);
        sorted.Sort(StringComparer.Ordinal);
        var text = new StringBuilder();
        text.Append('{');
        for (int i = 0; i < sorted.Count; ++i)
        {
            if (i != 0) { text.Append(','); }
            AppendJsonString(text, sorted[i]);
            text.Append(':');
            AppendJsonString(text, values[sorted[i]] ?? String.Empty);
        }
        text.Append('}');
        return text.ToString();
    }

    private static void AppendJsonString(StringBuilder text, string value)
    {
        text.Append('"');
        foreach (char c in value)
        {
            switch (c)
            {
                case '"': text.Append("\\\""); break;
                case '\\': text.Append("\\\\"); break;
                case '\b': text.Append("\\b"); break;
                case '\f': text.Append("\\f"); break;
                case '\n': text.Append("\\n"); break;
                case '\r': text.Append("\\r"); break;
                case '\t': text.Append("\\t"); break;
                default:
                    if (c < 0x20) { text.Append("\\u").Append(((int)c).ToString("x4", CultureInfo.InvariantCulture)); }
                    else { text.Append(c); }
                    break;
            }
        }
        text.Append('"');
    }

    private static Dictionary<string, string> ReadJson(string json)
    {
        int index = 0;
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        SkipSpace(json, ref index);
        Expect(json, ref index, '{');
        SkipSpace(json, ref index);
        if (index < json.Length && json[index] == '}') { ++index; return values; }
        while (true)
        {
            SkipSpace(json, ref index);
            string key = ReadJsonString(json, ref index);
            SkipSpace(json, ref index);
            Expect(json, ref index, ':');
            SkipSpace(json, ref index);
            string value = ReadJsonString(json, ref index);
            values[key] = value;
            SkipSpace(json, ref index);
            if (index >= json.Length) { throw new InvalidDataException("truncated JSON"); }
            if (json[index] == ',') { ++index; continue; }
            Expect(json, ref index, '}');
            return values;
        }
    }

    private static string ReadJsonString(string json, ref int index)
    {
        Expect(json, ref index, '"');
        var text = new StringBuilder();
        while (index < json.Length)
        {
            char c = json[index++];
            if (c == '"') { return text.ToString(); }
            if (c != '\\') { text.Append(c); continue; }
            if (index >= json.Length) { break; }
            char escape = json[index++];
            switch (escape)
            {
                case '"': text.Append('"'); break;
                case '\\': text.Append('\\'); break;
                case '/': text.Append('/'); break;
                case 'b': text.Append('\b'); break;
                case 'f': text.Append('\f'); break;
                case 'n': text.Append('\n'); break;
                case 'r': text.Append('\r'); break;
                case 't': text.Append('\t'); break;
                case 'u':
                    if (index + 4 > json.Length) { throw new InvalidDataException("bad \\u escape"); }
                    text.Append((char)int.Parse(json.Substring(index, 4), NumberStyles.HexNumber, CultureInfo.InvariantCulture));
                    index += 4;
                    break;
                default: throw new InvalidDataException("bad escape \\" + escape);
            }
        }
        throw new InvalidDataException("unterminated JSON string");
    }

    private static void SkipSpace(string json, ref int index)
    {
        while (index < json.Length && Char.IsWhiteSpace(json[index])) { ++index; }
    }

    private static void Expect(string json, ref int index, char expected)
    {
        if (index >= json.Length || json[index] != expected)
        {
            throw new InvalidDataException("expected '" + expected + "' at " + index.ToString(CultureInfo.InvariantCulture));
        }
        ++index;
    }
}
