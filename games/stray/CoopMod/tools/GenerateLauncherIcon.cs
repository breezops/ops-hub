// Deterministic image-to-ICO conversion; the supplied artwork is not redrawn.
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Collections.Generic;
internal static class GenerateLauncherIcon {
    static void Main(string[] args) {
        Directory.CreateDirectory(args[1]);
        int[] sizes={16,24,32,48,64,128,256};
        var frames=new List<byte[]>();
        using(var image=Image.FromFile(args[0])) {
            foreach(int size in sizes) {
                using(var bitmap=new Bitmap(size,size,PixelFormat.Format32bppArgb)) {
                    using(var graphics=Graphics.FromImage(bitmap)) {
                        graphics.InterpolationMode=InterpolationMode.HighQualityBicubic;
                        graphics.PixelOffsetMode=PixelOffsetMode.HighQuality;
                        graphics.DrawImage(image,new Rectangle(0,0,size,size));
                    }
                    if(size==256)bitmap.Save(Path.Combine(args[1],"launcher.png"),ImageFormat.Png);
                    var data=bitmap.LockBits(new Rectangle(0,0,size,size),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
                    byte[] pixels=new byte[data.Stride*size];Marshal.Copy(data.Scan0,pixels,0,pixels.Length);bitmap.UnlockBits(data);
                    using(var buffer=new MemoryStream())using(var w=new BinaryWriter(buffer)) {
                        w.Write(40);w.Write(size);w.Write(size*2);w.Write((short)1);w.Write((short)32);
                        w.Write(0);w.Write(pixels.Length);w.Write(0);w.Write(0);w.Write(0);w.Write(0);
                        for(int y=size-1;y>=0;y--)w.Write(pixels,y*data.Stride,size*4);
                        w.Write(new byte[((size+31)/32)*4*size]);frames.Add(buffer.ToArray());
                    }
                }
            }
        }
        using(var output=File.Create(Path.Combine(args[1],"launcher.ico")))using(var w=new BinaryWriter(output)) {
            w.Write((short)0);w.Write((short)1);w.Write((short)sizes.Length);
            int offset=6+16*sizes.Length;
            for(int i=0;i<sizes.Length;i++) {
                w.Write((byte)(sizes[i]==256?0:sizes[i]));w.Write((byte)(sizes[i]==256?0:sizes[i]));
                w.Write((byte)0);w.Write((byte)0);w.Write((short)1);w.Write((short)32);
                w.Write(frames[i].Length);w.Write(offset);offset+=frames[i].Length;
            }
            foreach(var frame in frames)w.Write(frame);
        }
    }
}
