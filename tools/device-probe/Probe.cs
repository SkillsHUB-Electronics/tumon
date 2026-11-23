// Tumon device probe.
//
// Lists every video capture device that Windows exposes through DirectShow and through Media Foundation,
// and every media type (pixel format, resolution, frame rate) each one offers. Read-only: it does not
// register, install or change anything. Opening a device briefly (to read its formats) may light a camera LED.
//
// Why: TeamTalk lists some cameras and not others. This tool shows what the cameras that work (for example
// Iriun) offer compared with those that do not (for example OBS Virtual Camera, recent SplitCam, laptop cameras).
// See docs/research/teamtalk-iriun-findings.md.
//
// Written in C# 5 on purpose so it compiles with the csc.exe that ships with Windows (no SDK needed).
// Build: tools\device-probe\build.ps1     Run: bin\probe-x64.exe [> result.md]
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Text;

namespace TumonProbe
{
    // ---------------------------------------------------------------- DirectShow interop
    [ComImport, Guid("62BE5D10-60EB-11d0-BD3B-00A0C911CE86")]
    class SystemDeviceEnumClass { }

    [ComImport, Guid("29840822-5B84-11d0-BD3B-00A0C911CE86"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface ICreateDevEnum
    {
        [PreserveSig] int CreateClassEnumerator(ref Guid pType, out IEnumMoniker ppEnumMoniker, int dwFlags);
    }

    [ComImport, Guid("55272A00-42CB-11CE-8135-00AA004BB851"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IPropertyBag
    {
        [PreserveSig] int Read([MarshalAs(UnmanagedType.LPWStr)] string pszPropName,
                               [MarshalAs(UnmanagedType.Struct)] out object pVar, IntPtr pErrorLog);
    }

    [ComImport, Guid("56a86895-0ad4-11ce-b03a-0020af0ba770"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IBaseFilter
    {
        void GetClassID(out Guid pClassID);
        [PreserveSig] int Stop();
        [PreserveSig] int Pause();
        [PreserveSig] int Run(long tStart);
        [PreserveSig] int GetState(int ms, out int state);
        [PreserveSig] int SetSyncSource(IntPtr clock);
        [PreserveSig] int GetSyncSource(out IntPtr clock);
        [PreserveSig] int EnumPins(out IEnumPins ppEnum);
    }

    [ComImport, Guid("56a86892-0ad4-11ce-b03a-0020af0ba770"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IEnumPins
    {
        [PreserveSig] int Next(int cPins, out IPin ppPins, IntPtr pcFetched);
    }

    [ComImport, Guid("56a86891-0ad4-11ce-b03a-0020af0ba770"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IPin
    {
        [PreserveSig] int Connect(IntPtr a, IntPtr b);
        [PreserveSig] int ReceiveConnection(IntPtr a, IntPtr b);
        [PreserveSig] int Disconnect();
        [PreserveSig] int ConnectedTo(out IntPtr a);
        [PreserveSig] int ConnectionMediaType(IntPtr a);
        [PreserveSig] int QueryPinInfo(IntPtr a);
        [PreserveSig] int QueryDirection(out int dir);
        [PreserveSig] int QueryId(out IntPtr id);
        [PreserveSig] int QueryAccept(IntPtr a);
        [PreserveSig] int EnumMediaTypes(out IEnumMediaTypes ppEnum);
    }

    [ComImport, Guid("89c31040-846b-11ce-97d3-00aa0055595a"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IEnumMediaTypes
    {
        [PreserveSig] int Next(int cMediaTypes, out IntPtr ppMediaTypes, IntPtr pcFetched);
    }

    [StructLayout(LayoutKind.Sequential)]
    struct AM_MEDIA_TYPE
    {
        public Guid majortype;
        public Guid subtype;
        public int bFixedSizeSamples;
        public int bTemporalCompression;
        public uint lSampleSize;
        public Guid formattype;
        public IntPtr pUnk;
        public uint cbFormat;
        public IntPtr pbFormat;
    }

    // ---------------------------------------------------------------- Media Foundation interop
    // Methods we never call are declared as placeholders so the vtable order stays right.
    [ComImport, Guid("2cd2d921-c447-44a7-a13c-4adabfc247e3"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMFAttributes
    {
        [PreserveSig] int GetItem(IntPtr a, IntPtr b);
        [PreserveSig] int GetItemType(IntPtr a, IntPtr b);
        [PreserveSig] int CompareItem(IntPtr a, IntPtr b, IntPtr c);
        [PreserveSig] int Compare(IntPtr a, int b, IntPtr c);
        [PreserveSig] int GetUINT32(ref Guid key, out uint value);
        [PreserveSig] int GetUINT64(ref Guid key, out ulong value);
        [PreserveSig] int GetDouble(IntPtr a, IntPtr b);
        [PreserveSig] int GetGUID(ref Guid key, out Guid value);
        [PreserveSig] int GetStringLength(IntPtr a, IntPtr b);
        [PreserveSig] int GetString(IntPtr a, IntPtr b, uint c, IntPtr d);
        [PreserveSig] int GetAllocatedString(ref Guid key, out IntPtr value, out uint length);
        [PreserveSig] int GetBlobSize(IntPtr a, IntPtr b);
        [PreserveSig] int GetBlob(IntPtr a, IntPtr b, uint c, IntPtr d);
        [PreserveSig] int GetAllocatedBlob(IntPtr a, IntPtr b, IntPtr c);
        [PreserveSig] int GetUnknown(IntPtr a, IntPtr b, IntPtr c);
        [PreserveSig] int SetItem(IntPtr a, IntPtr b);
        [PreserveSig] int DeleteItem(IntPtr a);
        [PreserveSig] int DeleteAllItems();
        [PreserveSig] int SetUINT32(ref Guid key, uint value);
        [PreserveSig] int SetUINT64(IntPtr a, ulong b);
        [PreserveSig] int SetDouble(IntPtr a, double b);
        [PreserveSig] int SetGUID(ref Guid key, ref Guid value);
        [PreserveSig] int SetString(IntPtr a, IntPtr b);
        [PreserveSig] int SetBlob(IntPtr a, IntPtr b, uint c);
        [PreserveSig] int SetUnknown(IntPtr a, IntPtr b);
        [PreserveSig] int LockStore();
        [PreserveSig] int UnlockStore();
        [PreserveSig] int GetCount(out uint count);
        [PreserveSig] int GetItemByIndex(IntPtr a, IntPtr b, IntPtr c);
        [PreserveSig] int CopyAllItems(IntPtr a);
    }

    [ComImport, Guid("7FEE9E9A-4A89-47a6-899C-B6A53A70FB67"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMFActivate : IMFAttributes
    {
        // IMFAttributes methods come first in the vtable.
        [PreserveSig] int ActivateObject(ref Guid riid, [MarshalAs(UnmanagedType.IUnknown)] out object ppv);
        [PreserveSig] int ShutdownObject();
        [PreserveSig] int DetachObject();
    }

    [ComImport, Guid("2CD0BD52-BCD5-4B89-B62C-EADC0C031E7D"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMFMediaEventGenerator
    {
        [PreserveSig] int GetEvent(uint flags, out IntPtr evt);
        [PreserveSig] int BeginGetEvent(IntPtr cb, IntPtr state);
        [PreserveSig] int EndGetEvent(IntPtr result, out IntPtr evt);
        [PreserveSig] int QueueEvent(uint type, ref Guid ext, int hr, IntPtr value);
    }

    [ComImport, Guid("279a808d-aec7-40c8-9c6b-a6b492c78a66"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMFMediaSource : IMFMediaEventGenerator
    {
        [PreserveSig] int GetCharacteristics(out uint characteristics);
        [PreserveSig] int CreatePresentationDescriptor(out IMFPresentationDescriptor ppPD);
        [PreserveSig] int Start(IntPtr pd, IntPtr timeFormat, IntPtr startPos);
        [PreserveSig] int Stop();
        [PreserveSig] int Pause();
        [PreserveSig] int Shutdown();
    }

    [ComImport, Guid("03cb2711-24d7-4db6-a17f-f3a7a479a536"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMFPresentationDescriptor : IMFAttributes
    {
        [PreserveSig] int GetStreamDescriptorCount(out uint count);
        [PreserveSig] int GetStreamDescriptorByIndex(uint index, out int selected, out IMFStreamDescriptor sd);
    }

    [ComImport, Guid("56c03d9c-9dbb-45f5-ab4b-d80f47c05938"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMFStreamDescriptor : IMFAttributes
    {
        [PreserveSig] int GetStreamIdentifier(out uint id);
        [PreserveSig] int GetMediaTypeHandler(out IMFMediaTypeHandler handler);
    }

    [ComImport, Guid("e93dcf6c-4b07-4e1e-8123-aa16ed6eadf5"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMFMediaTypeHandler
    {
        [PreserveSig] int IsMediaTypeSupported(IntPtr a, IntPtr b);
        [PreserveSig] int GetMediaTypeCount(out uint count);
        [PreserveSig] int GetMediaTypeByIndex(uint index, out IMFMediaType type);
    }

    [ComImport, Guid("44ae0fa8-ea31-4109-8d2e-4cae4997c555"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMFMediaType : IMFAttributes { }

    // ---------------------------------------------------------------- model
    class Offer
    {
        public string Subtype;
        public int Width, Height;
        public double Fps;
    }

    class DeviceInfo
    {
        public string Api;
        public string Name;
        public string Note;
        public List<Offer> Offers = new List<Offer>();
    }

    static class Program
    {
        [DllImport("ole32.dll")] static extern void CoTaskMemFree(IntPtr p);
        [DllImport("mfplat.dll")] static extern int MFStartup(uint version, uint flags);
        [DllImport("mfplat.dll")] static extern int MFShutdown();
        [DllImport("mfplat.dll")] static extern int MFCreateAttributes(out IMFAttributes attrs, uint size);
        [DllImport("mf.dll")] static extern int MFEnumDeviceSources(IMFAttributes attrs, out IntPtr sources, out uint count);

        static readonly Guid CLSID_VideoInputDeviceCategory = new Guid("860BB310-5D01-11d0-BD3B-00A0C911CE86");
        static readonly Guid IID_IPropertyBag = new Guid("55272A00-42CB-11CE-8135-00AA004BB851");
        static readonly Guid IID_IBaseFilter = new Guid("56a86895-0ad4-11ce-b03a-0020af0ba770");
        static readonly Guid FORMAT_VideoInfo = new Guid("05589f80-c356-11ce-bf01-00aa0055595a");
        static readonly Guid FORMAT_VideoInfo2 = new Guid("f72a76a0-eb0a-11d0-ace4-0000c0cc16ba");
        static readonly Guid MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE = new Guid("c60ac5fe-252a-478f-a0ef-bc8fa5f7cad3");
        static readonly Guid MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID = new Guid("8ac3587a-4ae7-42d8-99e0-0a6013eef90f");
        static readonly Guid MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME = new Guid("60d0e559-52f8-4fa2-bbce-acdb34a8ec01");
        static readonly Guid MF_MT_SUBTYPE = new Guid("f7e34c9a-42e8-4714-b74b-cb29d72c35e5");
        static readonly Guid MF_MT_FRAME_SIZE = new Guid("1652c33d-d6b2-4034-992e-ce8ee7e5ec1a");
        static readonly Guid MF_MT_FRAME_RATE = new Guid("c459a2e8-3d2c-4e44-b132-fee5156c7bb0");
        static readonly Guid IID_IMFMediaSource = new Guid("279a808d-aec7-40c8-9c6b-a6b492c78a66");

        [MTAThread]
        static int Main(string[] args)
        {
            List<DeviceInfo> all = new List<DeviceInfo>();
            string bits = IntPtr.Size == 8 ? "x64" : "x86";

            Console.OutputEncoding = Encoding.UTF8;
            Console.WriteLine("# Device probe (" + bits + " process)");
            Console.WriteLine();
            Console.WriteLine("- Windows: " + WindowsBuild());
            Console.WriteLine("- Run: " + DateTime.Now.ToString("yyyy-MM-dd HH:mm"));
            Console.WriteLine();

            try { all.AddRange(ProbeDirectShow()); }
            catch (Exception e) { Console.WriteLine("DirectShow probe failed: " + e.Message); }

            try { all.AddRange(ProbeMediaFoundation()); }
            catch (Exception e) { Console.WriteLine("Media Foundation probe failed: " + e.Message); }

            PrintSummary(all);
            Console.WriteLine();
            Console.WriteLine("## Details");
            foreach (DeviceInfo d in all) PrintDevice(d);
            return 0;
        }

        // ------------------------------------------------------------ DirectShow
        static List<DeviceInfo> ProbeDirectShow()
        {
            List<DeviceInfo> list = new List<DeviceInfo>();
            ICreateDevEnum devEnum = (ICreateDevEnum)new SystemDeviceEnumClass();
            Guid cat = CLSID_VideoInputDeviceCategory;
            IEnumMoniker em;
            int hr = devEnum.CreateClassEnumerator(ref cat, out em, 0);
            if (hr != 0 || em == null) return list;

            IMoniker[] mons = new IMoniker[1];
            while (em.Next(1, mons, IntPtr.Zero) == 0)
            {
                DeviceInfo di = new DeviceInfo();
                di.Api = "DirectShow";
                di.Name = "(unnamed)";
                try
                {
                    IMoniker mon = mons[0];
                    object bagObj;
                    Guid bagIid = IID_IPropertyBag;
                    mon.BindToStorage(null, null, ref bagIid, out bagObj);
                    object v;
                    ((IPropertyBag)bagObj).Read("FriendlyName", out v, IntPtr.Zero);
                    if (v != null) di.Name = CleanName(v.ToString());

                    object filterObj;
                    Guid filterIid = IID_IBaseFilter;
                    mon.BindToObject(null, null, ref filterIid, out filterObj);
                    ReadPins((IBaseFilter)filterObj, di);
                }
                catch (Exception e)
                {
                    di.Note = "could not open: " + e.Message.Trim();
                }
                list.Add(di);
            }
            return list;
        }

        static void ReadPins(IBaseFilter filter, DeviceInfo di)
        {
            IEnumPins ep;
            if (filter.EnumPins(out ep) != 0 || ep == null) { di.Note = "no pins"; return; }
            IPin pin;
            while (ep.Next(1, out pin, IntPtr.Zero) == 0 && pin != null)
            {
                int dir;
                if (pin.QueryDirection(out dir) != 0 || dir != 1) continue; // 1 = output pin
                IEnumMediaTypes emt;
                if (pin.EnumMediaTypes(out emt) != 0 || emt == null) continue;
                IntPtr pmt;
                while (emt.Next(1, out pmt, IntPtr.Zero) == 0 && pmt != IntPtr.Zero)
                {
                    AM_MEDIA_TYPE mt = (AM_MEDIA_TYPE)Marshal.PtrToStructure(pmt, typeof(AM_MEDIA_TYPE));
                    Offer o = new Offer();
                    o.Subtype = SubtypeName(mt.subtype);
                    if (mt.pbFormat != IntPtr.Zero)
                    {
                        int hdrOffset = -1;
                        if (mt.formattype == FORMAT_VideoInfo && mt.cbFormat >= 48 + 40) hdrOffset = 48;
                        else if (mt.formattype == FORMAT_VideoInfo2 && mt.cbFormat >= 72 + 40) hdrOffset = 72;
                        if (hdrOffset >= 0)
                        {
                            long avg = Marshal.ReadInt64(mt.pbFormat, 40);
                            o.Width = Marshal.ReadInt32(mt.pbFormat, hdrOffset + 4);
                            o.Height = Math.Abs(Marshal.ReadInt32(mt.pbFormat, hdrOffset + 8));
                            if (avg > 0) o.Fps = 10000000.0 / avg;
                        }
                    }
                    di.Offers.Add(o);
                    if (mt.pbFormat != IntPtr.Zero) CoTaskMemFree(mt.pbFormat);
                    if (mt.pUnk != IntPtr.Zero) Marshal.Release(mt.pUnk);
                    CoTaskMemFree(pmt);
                }
            }
        }

        // ------------------------------------------------------------ Media Foundation
        static List<DeviceInfo> ProbeMediaFoundation()
        {
            List<DeviceInfo> list = new List<DeviceInfo>();
            int hr = MFStartup(0x00020070, 0);
            if (hr != 0) throw new Exception("MFStartup failed 0x" + hr.ToString("X8"));
            try
            {
                IMFAttributes attrs;
                Step("MFCreateAttributes", MFCreateAttributes(out attrs, 1));
                Guid k = MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE;
                Guid v = MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID;
                Step("SetGUID(source type)", attrs.SetGUID(ref k, ref v));
                uint stored;
                attrs.GetCount(out stored);
                Console.Error.WriteLine("diag: attribute store holds " + stored + " item(s) before MFEnumDeviceSources");

                IntPtr arr; uint count;
                Step("MFEnumDeviceSources", MFEnumDeviceSources(attrs, out arr, out count));
                for (uint i = 0; i < count; i++)
                {
                    IntPtr p = Marshal.ReadIntPtr(arr, (int)(i * (uint)IntPtr.Size));
                    IMFActivate act = (IMFActivate)Marshal.GetObjectForIUnknown(p);
                    DeviceInfo di = new DeviceInfo();
                    di.Api = "MediaFoundation";
                    di.Name = CleanName(GetString(act, MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME) ?? "(unnamed)");
                    try { ReadMfTypes(act, di); }
                    catch (Exception e) { di.Note = "could not open: " + e.Message.Trim(); }
                    list.Add(di);
                }
                CoTaskMemFree(arr);
            }
            finally { MFShutdown(); }
            return list;
        }

        static void Step(string what, int hr)
        {
            if (hr < 0) throw new Exception(what + " failed: 0x" + hr.ToString("X8"));
        }

        static string GetString(IMFAttributes a, Guid key)
        {
            IntPtr s; uint len;
            if (a.GetAllocatedString(ref key, out s, out len) != 0) return null;
            string r = Marshal.PtrToStringUni(s);
            CoTaskMemFree(s);
            return r;
        }

        static void ReadMfTypes(IMFActivate act, DeviceInfo di)
        {
            Guid iid = IID_IMFMediaSource;
            object o;
            Step("ActivateObject", act.ActivateObject(ref iid, out o));
            IMFMediaSource src = (IMFMediaSource)o;
            try
            {
                IMFPresentationDescriptor pd;
                Step("CreatePresentationDescriptor", src.CreatePresentationDescriptor(out pd));
                uint n;
                Step("GetStreamDescriptorCount", pd.GetStreamDescriptorCount(out n));
                for (uint s = 0; s < n; s++)
                {
                    int sel; IMFStreamDescriptor sd;
                    if (pd.GetStreamDescriptorByIndex(s, out sel, out sd) != 0) continue;
                    IMFMediaTypeHandler h;
                    if (sd.GetMediaTypeHandler(out h) != 0) continue;
                    uint tc;
                    if (h.GetMediaTypeCount(out tc) != 0) continue;
                    for (uint t = 0; t < tc; t++)
                    {
                        IMFMediaType mt;
                        if (h.GetMediaTypeByIndex(t, out mt) != 0) continue;
                        Offer of = new Offer();
                        Guid sub; Guid ksub = MF_MT_SUBTYPE;
                        if (mt.GetGUID(ref ksub, out sub) == 0) of.Subtype = SubtypeName(sub); else of.Subtype = "?";
                        ulong size; Guid ksz = MF_MT_FRAME_SIZE;
                        if (mt.GetUINT64(ref ksz, out size) == 0) { of.Width = (int)(size >> 32); of.Height = (int)(size & 0xFFFFFFFF); }
                        ulong rate; Guid krt = MF_MT_FRAME_RATE;
                        if (mt.GetUINT64(ref krt, out rate) == 0)
                        {
                            uint num = (uint)(rate >> 32), den = (uint)(rate & 0xFFFFFFFF);
                            if (den != 0) of.Fps = (double)num / den;
                        }
                        di.Offers.Add(of);
                    }
                }
            }
            finally
            {
                src.Shutdown();
                act.ShutdownObject();
            }
        }

        // ------------------------------------------------------------ helpers and output
        static string SubtypeName(Guid g)
        {
            string s = g.ToString().ToLowerInvariant();
            if (s == "e436eb7d-524f-11ce-9f53-0020af0ba770") return "RGB24";
            if (s == "e436eb7e-524f-11ce-9f53-0020af0ba770") return "RGB32";
            if (s == "e436eb7c-524f-11ce-9f53-0020af0ba770") return "RGB555";
            if (s == "e436eb7b-524f-11ce-9f53-0020af0ba770") return "RGB565";
            if (s == "773c9ac0-3274-11d0-b724-00aa006c1a01") return "ARGB32";
            if (s.Substring(8) == "-0000-0010-8000-00aa00389b71")
            {
                byte[] b = g.ToByteArray();
                uint d1 = BitConverter.ToUInt32(b, 0);
                if (d1 == 0x14) return "RGB24";
                if (d1 == 0x16) return "RGB32";
                if (d1 == 0x15) return "ARGB32";
                bool printable = true;
                for (int i = 0; i < 4; i++) if (b[i] < 0x20 || b[i] > 0x7E) printable = false;
                if (printable) return Encoding.ASCII.GetString(b, 0, 4).Trim();
                return "FMT_" + d1;
            }
            return s;
        }

        static string WindowsBuild()
        {
            // Environment.OSVersion lies without an app manifest, so read the registry instead.
            try
            {
                using (Microsoft.Win32.RegistryKey k = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(
                    @"SOFTWARE\Microsoft\Windows NT\CurrentVersion"))
                {
                    return "build " + k.GetValue("CurrentBuild") + "." + k.GetValue("UBR") +
                           " (" + k.GetValue("DisplayVersion") + ")";
                }
            }
            catch (Exception) { return Environment.OSVersion.Version.ToString(); }
        }

        // Phones linked through "Windows Virtual Camera" are named after their owner; keep that out of results.
        static string CleanName(string name)
        {
            if (name != null && name.EndsWith("(Windows Virtual Camera)"))
                return "<linked phone> (Windows Virtual Camera)";
            return name;
        }

        static string Res(Offer o) { return o.Width + "x" + o.Height; }

        static void PrintSummary(List<DeviceInfo> all)
        {
            Console.WriteLine("## Summary");
            Console.WriteLine();
            Console.WriteLine("| API | Device | Formats | Largest size | Offers YUY2? | Offers only NV12/MJPG? | Note |");
            Console.WriteLine("|---|---|---|---|---|---|---|");
            foreach (DeviceInfo d in all)
            {
                SortedDictionary<string, int> fmts = new SortedDictionary<string, int>();
                long best = 0; string bestRes = "-";
                bool yuy2 = false, other = false;
                foreach (Offer o in d.Offers)
                {
                    if (!fmts.ContainsKey(o.Subtype)) fmts[o.Subtype] = 0;
                    fmts[o.Subtype]++;
                    long area = (long)o.Width * o.Height;
                    if (area > best) { best = area; bestRes = Res(o); }
                    if (o.Subtype == "YUY2" || o.Subtype == "YUYV") yuy2 = true;
                    if (o.Subtype != "NV12" && o.Subtype != "MJPG") other = true;
                }
                List<string> parts = new List<string>();
                foreach (KeyValuePair<string, int> kv in fmts) parts.Add(kv.Key + " (" + kv.Value + ")");
                string f = parts.Count > 0 ? string.Join(", ", parts.ToArray()) : "-";
                string only = d.Offers.Count == 0 ? "-" : (other ? "no" : "YES");
                Console.WriteLine("| " + d.Api + " | " + d.Name + " | " + f + " | " + bestRes + " | " +
                                  (d.Offers.Count == 0 ? "-" : (yuy2 ? "yes" : "no")) + " | " + only + " | " + (d.Note ?? "") + " |");
            }
        }

        static void PrintDevice(DeviceInfo d)
        {
            Console.WriteLine();
            Console.WriteLine("### " + d.Name + " (" + d.Api + ")");
            if (d.Note != null) Console.WriteLine("- " + d.Note);
            // group by subtype, then list sizes with the highest frame rate seen
            Dictionary<string, SortedDictionary<long, string>> g = new Dictionary<string, SortedDictionary<long, string>>();
            Dictionary<string, Dictionary<string, double>> maxFps = new Dictionary<string, Dictionary<string, double>>();
            List<string> order = new List<string>();
            foreach (Offer o in d.Offers)
            {
                if (!g.ContainsKey(o.Subtype)) { g[o.Subtype] = new SortedDictionary<long, string>(); maxFps[o.Subtype] = new Dictionary<string, double>(); order.Add(o.Subtype); }
                string r = Res(o);
                double cur;
                if (!maxFps[o.Subtype].TryGetValue(r, out cur) || o.Fps > cur) maxFps[o.Subtype][r] = o.Fps;
                g[o.Subtype][(long)o.Width * o.Height * 1000 + o.Width] = r;
            }
            foreach (string sub in order)
            {
                List<string> items = new List<string>();
                foreach (KeyValuePair<long, string> kv in g[sub])
                {
                    double fps = maxFps[sub][kv.Value];
                    items.Add(kv.Value + (fps > 0 ? "@" + Math.Round(fps) : ""));
                }
                Console.WriteLine("- **" + sub + "**: " + string.Join(", ", items.ToArray()));
            }
            if (d.Offers.Count == 0 && d.Note == null) Console.WriteLine("- (no media types reported)");
        }
    }
}
