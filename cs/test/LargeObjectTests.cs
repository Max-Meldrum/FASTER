﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Linq;
using FASTER.core;
using System.IO;
using NUnit.Framework;

namespace FASTER.test.largeobjects
{

    [TestFixture]
    internal class LargeObjectTests
    {
        private FasterKV<MyKey, MyLargeValue, MyInput, MyLargeOutput, Empty, MyLargeFunctions> fht1;
        private FasterKV<MyKey, MyLargeValue, MyInput, MyLargeOutput, Empty, MyLargeFunctions> fht2;
        private IDevice log, objlog;
        
        [Test]
        public void LargeObjectTest1()
        {
            MyInput input = default(MyInput);
            MyLargeOutput output = new MyLargeOutput();

            log = Devices.CreateLogDevice(TestContext.CurrentContext.TestDirectory + "\\hlog.log", deleteOnClose: true);
            objlog = Devices.CreateLogDevice(TestContext.CurrentContext.TestDirectory + "\\hlog.obj.log", deleteOnClose: true);

            Directory.CreateDirectory(TestContext.CurrentContext.TestDirectory + "\\checkpoints");

            fht1 = new FasterKV<MyKey, MyLargeValue, MyInput, MyLargeOutput, Empty, MyLargeFunctions>
                (128, new MyLargeFunctions(),
                new LogSettings { LogDevice = log, ObjectLogDevice = objlog, MutableFraction = 0.1, PageSizeBits = 21, MemorySizeBits = 26 },
                new CheckpointSettings { CheckpointDir = TestContext.CurrentContext.TestDirectory + "\\checkpoints", CheckPointType = CheckpointType.Snapshot },
                new SerializerSettings<MyKey, MyLargeValue> { keySerializer = () => new MyKeySerializer(), valueSerializer = () => new MyLargeValueSerializer() }
                );

            fht2 = new FasterKV<MyKey, MyLargeValue, MyInput, MyLargeOutput, Empty, MyLargeFunctions>
                (128, new MyLargeFunctions(),
                new LogSettings { LogDevice = log, ObjectLogDevice = objlog, MutableFraction = 0.1, PageSizeBits = 21, MemorySizeBits = 26 },
                new CheckpointSettings { CheckpointDir = TestContext.CurrentContext.TestDirectory + "\\checkpoints", CheckPointType = CheckpointType.Snapshot },
                new SerializerSettings<MyKey, MyLargeValue> { keySerializer = () => new MyKeySerializer(), valueSerializer = () => new MyLargeValueSerializer() }
                );

            int maxSize = 100;
            int numOps = 5000;

            fht1.StartSession();
            Random r = new Random(33);
            for (int key = 0; key < numOps; key++)
            {
                var mykey = new MyKey { key = key };
                var value = new MyLargeValue(1+r.Next(maxSize));
                fht1.Upsert(ref mykey, ref value, Empty.Default, 0);
            }
            fht1.TakeFullCheckpoint(out Guid token);
            fht1.CompleteCheckpoint(true);
            fht1.StopSession();
            fht1.Dispose();

            fht2.Recover(token);
            fht2.StartSession();
            for (int keycnt = 0; keycnt < numOps; keycnt++)
            {
                var key = new MyKey { key = keycnt };
                var status = fht2.Read(ref key, ref input, ref output, Empty.Default, 0);

                if (status == Status.PENDING)
                    fht2.CompletePending(true);
                else
                {
                    for (int i = 0; i < output.value.value.Length; i++)
                    {
                        Assert.IsTrue(output.value.value[i] == (byte)(output.value.value.Length+i));
                    }
                }
            }
            fht2.StopSession();
            fht2.Dispose();

            log.Close();
            objlog.Close();
            new DirectoryInfo(TestContext.CurrentContext.TestDirectory + "\\checkpoints").Delete(true);
        }

        [Test]
        public void LargeObjectTest2()
        {
            MyInput input = default(MyInput);
            MyLargeOutput output = new MyLargeOutput();

            log = Devices.CreateLogDevice(TestContext.CurrentContext.TestDirectory + "\\hlog.log", deleteOnClose: true);
            objlog = Devices.CreateLogDevice(TestContext.CurrentContext.TestDirectory + "\\hlog.obj.log", deleteOnClose: true);

            Directory.CreateDirectory(TestContext.CurrentContext.TestDirectory + "\\checkpoints");

            fht1 = new FasterKV<MyKey, MyLargeValue, MyInput, MyLargeOutput, Empty, MyLargeFunctions>
                (128, new MyLargeFunctions(),
                new LogSettings { LogDevice = log, ObjectLogDevice = objlog, MutableFraction = 0.1, PageSizeBits = 21, MemorySizeBits = 26 },
                new CheckpointSettings { CheckpointDir = TestContext.CurrentContext.TestDirectory + "\\checkpoints", CheckPointType = CheckpointType.FoldOver },
                new SerializerSettings<MyKey, MyLargeValue> { keySerializer = () => new MyKeySerializer(), valueSerializer = () => new MyLargeValueSerializer() }
                );

            fht2 = new FasterKV<MyKey, MyLargeValue, MyInput, MyLargeOutput, Empty, MyLargeFunctions>
                (128, new MyLargeFunctions(),
                new LogSettings { LogDevice = log, ObjectLogDevice = objlog, MutableFraction = 0.1, PageSizeBits = 21, MemorySizeBits = 26 },
                new CheckpointSettings { CheckpointDir = TestContext.CurrentContext.TestDirectory + "\\checkpoints", CheckPointType = CheckpointType.FoldOver },
                new SerializerSettings<MyKey, MyLargeValue> { keySerializer = () => new MyKeySerializer(), valueSerializer = () => new MyLargeValueSerializer() }
                );

            int maxSize = 100;
            int numOps = 5000;

            fht1.StartSession();
            Random r = new Random(33);
            for (int key = 0; key < numOps; key++)
            {
                var mykey = new MyKey { key = key };
                var value = new MyLargeValue(1 + r.Next(maxSize));
                fht1.Upsert(ref mykey, ref value, Empty.Default, 0);
            }
            fht1.TakeFullCheckpoint(out Guid token);
            fht1.CompleteCheckpoint(true);
            fht1.StopSession();
            fht1.Dispose();

            fht2.Recover(token);
            fht2.StartSession();
            for (int keycnt = 0; keycnt < numOps; keycnt++)
            {
                var key = new MyKey { key = keycnt };
                var status = fht2.Read(ref key, ref input, ref output, Empty.Default, 0);

                if (status == Status.PENDING)
                    fht2.CompletePending(true);
                else
                {
                    for (int i = 0; i < output.value.value.Length; i++)
                    {
                        Assert.IsTrue(output.value.value[i] == (byte)(output.value.value.Length + i));
                    }
                }
            }
            fht2.StopSession();
            fht2.Dispose();

            log.Close();
            objlog.Close();
            new DirectoryInfo(TestContext.CurrentContext.TestDirectory + "\\checkpoints").Delete(true);
        }
    }
}
