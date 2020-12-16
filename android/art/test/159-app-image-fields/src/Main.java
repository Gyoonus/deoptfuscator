/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import AAA.Derived;

public class Main {
    public static void main(String[] args) {
        try {
            // Make sure we resolve Fields before eating memory.
            // (Making sure that the test passes in no-image configurations.)
            Class.forName("Fields", false, Main.class.getClassLoader());
            System.out.println("Eating all memory.");
            Object memory = eatAllMemory();

            // This test assumes that Derived is not yet resolved. In some configurations
            // (notably interp-ac), Derived is already resolved by verifying Main at run
            // time. Therefore we cannot assume that we get a certain `value` and need to
            // simply check for consistency, i.e. `value == another_value`.
            int value = 0;
            try {
                // If the ArtField* is erroneously left in the DexCache, this
                // shall succeed despite the class Derived being unresolved so
                // far. Otherwise, we shall throw OOME trying to resolve it.
                value = Derived.value;
            } catch (OutOfMemoryError e) {
                value = -1;
            }
            Fields.clobberDexCache();
            int another_value = 0;
            try {
                // Try again for comparison. Since the DexCache field array has been
                // clobbered by Fields.clobberDexCache(), this shall throw OOME.
                another_value = Derived.value;
            } catch (OutOfMemoryError e) {
                another_value = -1;
            }
            boolean memoryWasAllocated = (memory != null);
            memory = null;
            System.out.println("memoryWasAllocated = " + memoryWasAllocated);
            System.out.println("match: " + (value == another_value));
            if (value != another_value || (value != -1 && value != 42)) {
                // Mismatch or unexpected value, print additional debugging information.
                System.out.println("value: " + value);
                System.out.println("another_value: " + another_value);
            }
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
    }

    public static Object eatAllMemory() {
      Object[] result = null;
      int size = 1000000;
      while (result == null && size != 0) {
          try {
              result = new Object[size];
          } catch (OutOfMemoryError oome) {
              size /= 2;
          }
      }
      if (result != null) {
          int index = 0;
          while (index != result.length && size != 0) {
              try {
                  result[index] = new byte[size];
                  ++index;
              } catch (OutOfMemoryError oome) {
                  size /= 2;
              }
          }
      }
      return result;
  }
}

// The naming is deliberate to take into account two different situations:
//   - eagerly preloading DexCache with the available candidate with the lowest index,
//   - not preloading DexCache and relying on the verification to populate it.
// This corresponds to new and old behavior, respectively.
//
// Eager preloading: "LFields;" is after "LAAA/Base;" and "LAAA/Derived;" so that
// Derived.value takes priority over Fields.testField*.
//
// Relying on verifier: "LFields;" is before "LMain;" so that the class definition
// of Fields precedes the definition of Main (this is not strictly required but the
// tools look at lexicographic ordering when there is no inheritance relationship)
// and the verification of Main is last and fills the DexCache with Derived.value.
//
class Fields {
    public static int clobberDexCache() {
        return 0
                + testField0000
                + testField0001
                + testField0002
                + testField0003
                + testField0004
                + testField0005
                + testField0006
                + testField0007
                + testField0008
                + testField0009
                + testField0010
                + testField0011
                + testField0012
                + testField0013
                + testField0014
                + testField0015
                + testField0016
                + testField0017
                + testField0018
                + testField0019
                + testField0020
                + testField0021
                + testField0022
                + testField0023
                + testField0024
                + testField0025
                + testField0026
                + testField0027
                + testField0028
                + testField0029
                + testField0030
                + testField0031
                + testField0032
                + testField0033
                + testField0034
                + testField0035
                + testField0036
                + testField0037
                + testField0038
                + testField0039
                + testField0040
                + testField0041
                + testField0042
                + testField0043
                + testField0044
                + testField0045
                + testField0046
                + testField0047
                + testField0048
                + testField0049
                + testField0050
                + testField0051
                + testField0052
                + testField0053
                + testField0054
                + testField0055
                + testField0056
                + testField0057
                + testField0058
                + testField0059
                + testField0060
                + testField0061
                + testField0062
                + testField0063
                + testField0064
                + testField0065
                + testField0066
                + testField0067
                + testField0068
                + testField0069
                + testField0070
                + testField0071
                + testField0072
                + testField0073
                + testField0074
                + testField0075
                + testField0076
                + testField0077
                + testField0078
                + testField0079
                + testField0080
                + testField0081
                + testField0082
                + testField0083
                + testField0084
                + testField0085
                + testField0086
                + testField0087
                + testField0088
                + testField0089
                + testField0090
                + testField0091
                + testField0092
                + testField0093
                + testField0094
                + testField0095
                + testField0096
                + testField0097
                + testField0098
                + testField0099
                + testField0100
                + testField0101
                + testField0102
                + testField0103
                + testField0104
                + testField0105
                + testField0106
                + testField0107
                + testField0108
                + testField0109
                + testField0110
                + testField0111
                + testField0112
                + testField0113
                + testField0114
                + testField0115
                + testField0116
                + testField0117
                + testField0118
                + testField0119
                + testField0120
                + testField0121
                + testField0122
                + testField0123
                + testField0124
                + testField0125
                + testField0126
                + testField0127
                + testField0128
                + testField0129
                + testField0130
                + testField0131
                + testField0132
                + testField0133
                + testField0134
                + testField0135
                + testField0136
                + testField0137
                + testField0138
                + testField0139
                + testField0140
                + testField0141
                + testField0142
                + testField0143
                + testField0144
                + testField0145
                + testField0146
                + testField0147
                + testField0148
                + testField0149
                + testField0150
                + testField0151
                + testField0152
                + testField0153
                + testField0154
                + testField0155
                + testField0156
                + testField0157
                + testField0158
                + testField0159
                + testField0160
                + testField0161
                + testField0162
                + testField0163
                + testField0164
                + testField0165
                + testField0166
                + testField0167
                + testField0168
                + testField0169
                + testField0170
                + testField0171
                + testField0172
                + testField0173
                + testField0174
                + testField0175
                + testField0176
                + testField0177
                + testField0178
                + testField0179
                + testField0180
                + testField0181
                + testField0182
                + testField0183
                + testField0184
                + testField0185
                + testField0186
                + testField0187
                + testField0188
                + testField0189
                + testField0190
                + testField0191
                + testField0192
                + testField0193
                + testField0194
                + testField0195
                + testField0196
                + testField0197
                + testField0198
                + testField0199
                + testField0200
                + testField0201
                + testField0202
                + testField0203
                + testField0204
                + testField0205
                + testField0206
                + testField0207
                + testField0208
                + testField0209
                + testField0210
                + testField0211
                + testField0212
                + testField0213
                + testField0214
                + testField0215
                + testField0216
                + testField0217
                + testField0218
                + testField0219
                + testField0220
                + testField0221
                + testField0222
                + testField0223
                + testField0224
                + testField0225
                + testField0226
                + testField0227
                + testField0228
                + testField0229
                + testField0230
                + testField0231
                + testField0232
                + testField0233
                + testField0234
                + testField0235
                + testField0236
                + testField0237
                + testField0238
                + testField0239
                + testField0240
                + testField0241
                + testField0242
                + testField0243
                + testField0244
                + testField0245
                + testField0246
                + testField0247
                + testField0248
                + testField0249
                + testField0250
                + testField0251
                + testField0252
                + testField0253
                + testField0254
                + testField0255
                + testField0256
                + testField0257
                + testField0258
                + testField0259
                + testField0260
                + testField0261
                + testField0262
                + testField0263
                + testField0264
                + testField0265
                + testField0266
                + testField0267
                + testField0268
                + testField0269
                + testField0270
                + testField0271
                + testField0272
                + testField0273
                + testField0274
                + testField0275
                + testField0276
                + testField0277
                + testField0278
                + testField0279
                + testField0280
                + testField0281
                + testField0282
                + testField0283
                + testField0284
                + testField0285
                + testField0286
                + testField0287
                + testField0288
                + testField0289
                + testField0290
                + testField0291
                + testField0292
                + testField0293
                + testField0294
                + testField0295
                + testField0296
                + testField0297
                + testField0298
                + testField0299
                + testField0300
                + testField0301
                + testField0302
                + testField0303
                + testField0304
                + testField0305
                + testField0306
                + testField0307
                + testField0308
                + testField0309
                + testField0310
                + testField0311
                + testField0312
                + testField0313
                + testField0314
                + testField0315
                + testField0316
                + testField0317
                + testField0318
                + testField0319
                + testField0320
                + testField0321
                + testField0322
                + testField0323
                + testField0324
                + testField0325
                + testField0326
                + testField0327
                + testField0328
                + testField0329
                + testField0330
                + testField0331
                + testField0332
                + testField0333
                + testField0334
                + testField0335
                + testField0336
                + testField0337
                + testField0338
                + testField0339
                + testField0340
                + testField0341
                + testField0342
                + testField0343
                + testField0344
                + testField0345
                + testField0346
                + testField0347
                + testField0348
                + testField0349
                + testField0350
                + testField0351
                + testField0352
                + testField0353
                + testField0354
                + testField0355
                + testField0356
                + testField0357
                + testField0358
                + testField0359
                + testField0360
                + testField0361
                + testField0362
                + testField0363
                + testField0364
                + testField0365
                + testField0366
                + testField0367
                + testField0368
                + testField0369
                + testField0370
                + testField0371
                + testField0372
                + testField0373
                + testField0374
                + testField0375
                + testField0376
                + testField0377
                + testField0378
                + testField0379
                + testField0380
                + testField0381
                + testField0382
                + testField0383
                + testField0384
                + testField0385
                + testField0386
                + testField0387
                + testField0388
                + testField0389
                + testField0390
                + testField0391
                + testField0392
                + testField0393
                + testField0394
                + testField0395
                + testField0396
                + testField0397
                + testField0398
                + testField0399
                + testField0400
                + testField0401
                + testField0402
                + testField0403
                + testField0404
                + testField0405
                + testField0406
                + testField0407
                + testField0408
                + testField0409
                + testField0410
                + testField0411
                + testField0412
                + testField0413
                + testField0414
                + testField0415
                + testField0416
                + testField0417
                + testField0418
                + testField0419
                + testField0420
                + testField0421
                + testField0422
                + testField0423
                + testField0424
                + testField0425
                + testField0426
                + testField0427
                + testField0428
                + testField0429
                + testField0430
                + testField0431
                + testField0432
                + testField0433
                + testField0434
                + testField0435
                + testField0436
                + testField0437
                + testField0438
                + testField0439
                + testField0440
                + testField0441
                + testField0442
                + testField0443
                + testField0444
                + testField0445
                + testField0446
                + testField0447
                + testField0448
                + testField0449
                + testField0450
                + testField0451
                + testField0452
                + testField0453
                + testField0454
                + testField0455
                + testField0456
                + testField0457
                + testField0458
                + testField0459
                + testField0460
                + testField0461
                + testField0462
                + testField0463
                + testField0464
                + testField0465
                + testField0466
                + testField0467
                + testField0468
                + testField0469
                + testField0470
                + testField0471
                + testField0472
                + testField0473
                + testField0474
                + testField0475
                + testField0476
                + testField0477
                + testField0478
                + testField0479
                + testField0480
                + testField0481
                + testField0482
                + testField0483
                + testField0484
                + testField0485
                + testField0486
                + testField0487
                + testField0488
                + testField0489
                + testField0490
                + testField0491
                + testField0492
                + testField0493
                + testField0494
                + testField0495
                + testField0496
                + testField0497
                + testField0498
                + testField0499
                + testField0500
                + testField0501
                + testField0502
                + testField0503
                + testField0504
                + testField0505
                + testField0506
                + testField0507
                + testField0508
                + testField0509
                + testField0510
                + testField0511
                + testField0512
                + testField0513
                + testField0514
                + testField0515
                + testField0516
                + testField0517
                + testField0518
                + testField0519
                + testField0520
                + testField0521
                + testField0522
                + testField0523
                + testField0524
                + testField0525
                + testField0526
                + testField0527
                + testField0528
                + testField0529
                + testField0530
                + testField0531
                + testField0532
                + testField0533
                + testField0534
                + testField0535
                + testField0536
                + testField0537
                + testField0538
                + testField0539
                + testField0540
                + testField0541
                + testField0542
                + testField0543
                + testField0544
                + testField0545
                + testField0546
                + testField0547
                + testField0548
                + testField0549
                + testField0550
                + testField0551
                + testField0552
                + testField0553
                + testField0554
                + testField0555
                + testField0556
                + testField0557
                + testField0558
                + testField0559
                + testField0560
                + testField0561
                + testField0562
                + testField0563
                + testField0564
                + testField0565
                + testField0566
                + testField0567
                + testField0568
                + testField0569
                + testField0570
                + testField0571
                + testField0572
                + testField0573
                + testField0574
                + testField0575
                + testField0576
                + testField0577
                + testField0578
                + testField0579
                + testField0580
                + testField0581
                + testField0582
                + testField0583
                + testField0584
                + testField0585
                + testField0586
                + testField0587
                + testField0588
                + testField0589
                + testField0590
                + testField0591
                + testField0592
                + testField0593
                + testField0594
                + testField0595
                + testField0596
                + testField0597
                + testField0598
                + testField0599
                + testField0600
                + testField0601
                + testField0602
                + testField0603
                + testField0604
                + testField0605
                + testField0606
                + testField0607
                + testField0608
                + testField0609
                + testField0610
                + testField0611
                + testField0612
                + testField0613
                + testField0614
                + testField0615
                + testField0616
                + testField0617
                + testField0618
                + testField0619
                + testField0620
                + testField0621
                + testField0622
                + testField0623
                + testField0624
                + testField0625
                + testField0626
                + testField0627
                + testField0628
                + testField0629
                + testField0630
                + testField0631
                + testField0632
                + testField0633
                + testField0634
                + testField0635
                + testField0636
                + testField0637
                + testField0638
                + testField0639
                + testField0640
                + testField0641
                + testField0642
                + testField0643
                + testField0644
                + testField0645
                + testField0646
                + testField0647
                + testField0648
                + testField0649
                + testField0650
                + testField0651
                + testField0652
                + testField0653
                + testField0654
                + testField0655
                + testField0656
                + testField0657
                + testField0658
                + testField0659
                + testField0660
                + testField0661
                + testField0662
                + testField0663
                + testField0664
                + testField0665
                + testField0666
                + testField0667
                + testField0668
                + testField0669
                + testField0670
                + testField0671
                + testField0672
                + testField0673
                + testField0674
                + testField0675
                + testField0676
                + testField0677
                + testField0678
                + testField0679
                + testField0680
                + testField0681
                + testField0682
                + testField0683
                + testField0684
                + testField0685
                + testField0686
                + testField0687
                + testField0688
                + testField0689
                + testField0690
                + testField0691
                + testField0692
                + testField0693
                + testField0694
                + testField0695
                + testField0696
                + testField0697
                + testField0698
                + testField0699
                + testField0700
                + testField0701
                + testField0702
                + testField0703
                + testField0704
                + testField0705
                + testField0706
                + testField0707
                + testField0708
                + testField0709
                + testField0710
                + testField0711
                + testField0712
                + testField0713
                + testField0714
                + testField0715
                + testField0716
                + testField0717
                + testField0718
                + testField0719
                + testField0720
                + testField0721
                + testField0722
                + testField0723
                + testField0724
                + testField0725
                + testField0726
                + testField0727
                + testField0728
                + testField0729
                + testField0730
                + testField0731
                + testField0732
                + testField0733
                + testField0734
                + testField0735
                + testField0736
                + testField0737
                + testField0738
                + testField0739
                + testField0740
                + testField0741
                + testField0742
                + testField0743
                + testField0744
                + testField0745
                + testField0746
                + testField0747
                + testField0748
                + testField0749
                + testField0750
                + testField0751
                + testField0752
                + testField0753
                + testField0754
                + testField0755
                + testField0756
                + testField0757
                + testField0758
                + testField0759
                + testField0760
                + testField0761
                + testField0762
                + testField0763
                + testField0764
                + testField0765
                + testField0766
                + testField0767
                + testField0768
                + testField0769
                + testField0770
                + testField0771
                + testField0772
                + testField0773
                + testField0774
                + testField0775
                + testField0776
                + testField0777
                + testField0778
                + testField0779
                + testField0780
                + testField0781
                + testField0782
                + testField0783
                + testField0784
                + testField0785
                + testField0786
                + testField0787
                + testField0788
                + testField0789
                + testField0790
                + testField0791
                + testField0792
                + testField0793
                + testField0794
                + testField0795
                + testField0796
                + testField0797
                + testField0798
                + testField0799
                + testField0800
                + testField0801
                + testField0802
                + testField0803
                + testField0804
                + testField0805
                + testField0806
                + testField0807
                + testField0808
                + testField0809
                + testField0810
                + testField0811
                + testField0812
                + testField0813
                + testField0814
                + testField0815
                + testField0816
                + testField0817
                + testField0818
                + testField0819
                + testField0820
                + testField0821
                + testField0822
                + testField0823
                + testField0824
                + testField0825
                + testField0826
                + testField0827
                + testField0828
                + testField0829
                + testField0830
                + testField0831
                + testField0832
                + testField0833
                + testField0834
                + testField0835
                + testField0836
                + testField0837
                + testField0838
                + testField0839
                + testField0840
                + testField0841
                + testField0842
                + testField0843
                + testField0844
                + testField0845
                + testField0846
                + testField0847
                + testField0848
                + testField0849
                + testField0850
                + testField0851
                + testField0852
                + testField0853
                + testField0854
                + testField0855
                + testField0856
                + testField0857
                + testField0858
                + testField0859
                + testField0860
                + testField0861
                + testField0862
                + testField0863
                + testField0864
                + testField0865
                + testField0866
                + testField0867
                + testField0868
                + testField0869
                + testField0870
                + testField0871
                + testField0872
                + testField0873
                + testField0874
                + testField0875
                + testField0876
                + testField0877
                + testField0878
                + testField0879
                + testField0880
                + testField0881
                + testField0882
                + testField0883
                + testField0884
                + testField0885
                + testField0886
                + testField0887
                + testField0888
                + testField0889
                + testField0890
                + testField0891
                + testField0892
                + testField0893
                + testField0894
                + testField0895
                + testField0896
                + testField0897
                + testField0898
                + testField0899
                + testField0900
                + testField0901
                + testField0902
                + testField0903
                + testField0904
                + testField0905
                + testField0906
                + testField0907
                + testField0908
                + testField0909
                + testField0910
                + testField0911
                + testField0912
                + testField0913
                + testField0914
                + testField0915
                + testField0916
                + testField0917
                + testField0918
                + testField0919
                + testField0920
                + testField0921
                + testField0922
                + testField0923
                + testField0924
                + testField0925
                + testField0926
                + testField0927
                + testField0928
                + testField0929
                + testField0930
                + testField0931
                + testField0932
                + testField0933
                + testField0934
                + testField0935
                + testField0936
                + testField0937
                + testField0938
                + testField0939
                + testField0940
                + testField0941
                + testField0942
                + testField0943
                + testField0944
                + testField0945
                + testField0946
                + testField0947
                + testField0948
                + testField0949
                + testField0950
                + testField0951
                + testField0952
                + testField0953
                + testField0954
                + testField0955
                + testField0956
                + testField0957
                + testField0958
                + testField0959
                + testField0960
                + testField0961
                + testField0962
                + testField0963
                + testField0964
                + testField0965
                + testField0966
                + testField0967
                + testField0968
                + testField0969
                + testField0970
                + testField0971
                + testField0972
                + testField0973
                + testField0974
                + testField0975
                + testField0976
                + testField0977
                + testField0978
                + testField0979
                + testField0980
                + testField0981
                + testField0982
                + testField0983
                + testField0984
                + testField0985
                + testField0986
                + testField0987
                + testField0988
                + testField0989
                + testField0990
                + testField0991
                + testField0992
                + testField0993
                + testField0994
                + testField0995
                + testField0996
                + testField0997
                + testField0998
                + testField0999
                + testField1000
                + testField1001
                + testField1002
                + testField1003
                + testField1004
                + testField1005
                + testField1006
                + testField1007
                + testField1008
                + testField1009
                + testField1010
                + testField1011
                + testField1012
                + testField1013
                + testField1014
                + testField1015
                + testField1016
                + testField1017
                + testField1018
                + testField1019
                + testField1020
                + testField1021
                + testField1022
                + testField1023
                + 0;
    }

    private static int testField0000 = 0;
    private static int testField0001 = 1;
    private static int testField0002 = 2;
    private static int testField0003 = 3;
    private static int testField0004 = 4;
    private static int testField0005 = 5;
    private static int testField0006 = 6;
    private static int testField0007 = 7;
    private static int testField0008 = 8;
    private static int testField0009 = 9;
    private static int testField0010 = 10;
    private static int testField0011 = 11;
    private static int testField0012 = 12;
    private static int testField0013 = 13;
    private static int testField0014 = 14;
    private static int testField0015 = 15;
    private static int testField0016 = 16;
    private static int testField0017 = 17;
    private static int testField0018 = 18;
    private static int testField0019 = 19;
    private static int testField0020 = 20;
    private static int testField0021 = 21;
    private static int testField0022 = 22;
    private static int testField0023 = 23;
    private static int testField0024 = 24;
    private static int testField0025 = 25;
    private static int testField0026 = 26;
    private static int testField0027 = 27;
    private static int testField0028 = 28;
    private static int testField0029 = 29;
    private static int testField0030 = 30;
    private static int testField0031 = 31;
    private static int testField0032 = 32;
    private static int testField0033 = 33;
    private static int testField0034 = 34;
    private static int testField0035 = 35;
    private static int testField0036 = 36;
    private static int testField0037 = 37;
    private static int testField0038 = 38;
    private static int testField0039 = 39;
    private static int testField0040 = 40;
    private static int testField0041 = 41;
    private static int testField0042 = 42;
    private static int testField0043 = 43;
    private static int testField0044 = 44;
    private static int testField0045 = 45;
    private static int testField0046 = 46;
    private static int testField0047 = 47;
    private static int testField0048 = 48;
    private static int testField0049 = 49;
    private static int testField0050 = 50;
    private static int testField0051 = 51;
    private static int testField0052 = 52;
    private static int testField0053 = 53;
    private static int testField0054 = 54;
    private static int testField0055 = 55;
    private static int testField0056 = 56;
    private static int testField0057 = 57;
    private static int testField0058 = 58;
    private static int testField0059 = 59;
    private static int testField0060 = 60;
    private static int testField0061 = 61;
    private static int testField0062 = 62;
    private static int testField0063 = 63;
    private static int testField0064 = 64;
    private static int testField0065 = 65;
    private static int testField0066 = 66;
    private static int testField0067 = 67;
    private static int testField0068 = 68;
    private static int testField0069 = 69;
    private static int testField0070 = 70;
    private static int testField0071 = 71;
    private static int testField0072 = 72;
    private static int testField0073 = 73;
    private static int testField0074 = 74;
    private static int testField0075 = 75;
    private static int testField0076 = 76;
    private static int testField0077 = 77;
    private static int testField0078 = 78;
    private static int testField0079 = 79;
    private static int testField0080 = 80;
    private static int testField0081 = 81;
    private static int testField0082 = 82;
    private static int testField0083 = 83;
    private static int testField0084 = 84;
    private static int testField0085 = 85;
    private static int testField0086 = 86;
    private static int testField0087 = 87;
    private static int testField0088 = 88;
    private static int testField0089 = 89;
    private static int testField0090 = 90;
    private static int testField0091 = 91;
    private static int testField0092 = 92;
    private static int testField0093 = 93;
    private static int testField0094 = 94;
    private static int testField0095 = 95;
    private static int testField0096 = 96;
    private static int testField0097 = 97;
    private static int testField0098 = 98;
    private static int testField0099 = 99;
    private static int testField0100 = 100;
    private static int testField0101 = 101;
    private static int testField0102 = 102;
    private static int testField0103 = 103;
    private static int testField0104 = 104;
    private static int testField0105 = 105;
    private static int testField0106 = 106;
    private static int testField0107 = 107;
    private static int testField0108 = 108;
    private static int testField0109 = 109;
    private static int testField0110 = 110;
    private static int testField0111 = 111;
    private static int testField0112 = 112;
    private static int testField0113 = 113;
    private static int testField0114 = 114;
    private static int testField0115 = 115;
    private static int testField0116 = 116;
    private static int testField0117 = 117;
    private static int testField0118 = 118;
    private static int testField0119 = 119;
    private static int testField0120 = 120;
    private static int testField0121 = 121;
    private static int testField0122 = 122;
    private static int testField0123 = 123;
    private static int testField0124 = 124;
    private static int testField0125 = 125;
    private static int testField0126 = 126;
    private static int testField0127 = 127;
    private static int testField0128 = 128;
    private static int testField0129 = 129;
    private static int testField0130 = 130;
    private static int testField0131 = 131;
    private static int testField0132 = 132;
    private static int testField0133 = 133;
    private static int testField0134 = 134;
    private static int testField0135 = 135;
    private static int testField0136 = 136;
    private static int testField0137 = 137;
    private static int testField0138 = 138;
    private static int testField0139 = 139;
    private static int testField0140 = 140;
    private static int testField0141 = 141;
    private static int testField0142 = 142;
    private static int testField0143 = 143;
    private static int testField0144 = 144;
    private static int testField0145 = 145;
    private static int testField0146 = 146;
    private static int testField0147 = 147;
    private static int testField0148 = 148;
    private static int testField0149 = 149;
    private static int testField0150 = 150;
    private static int testField0151 = 151;
    private static int testField0152 = 152;
    private static int testField0153 = 153;
    private static int testField0154 = 154;
    private static int testField0155 = 155;
    private static int testField0156 = 156;
    private static int testField0157 = 157;
    private static int testField0158 = 158;
    private static int testField0159 = 159;
    private static int testField0160 = 160;
    private static int testField0161 = 161;
    private static int testField0162 = 162;
    private static int testField0163 = 163;
    private static int testField0164 = 164;
    private static int testField0165 = 165;
    private static int testField0166 = 166;
    private static int testField0167 = 167;
    private static int testField0168 = 168;
    private static int testField0169 = 169;
    private static int testField0170 = 170;
    private static int testField0171 = 171;
    private static int testField0172 = 172;
    private static int testField0173 = 173;
    private static int testField0174 = 174;
    private static int testField0175 = 175;
    private static int testField0176 = 176;
    private static int testField0177 = 177;
    private static int testField0178 = 178;
    private static int testField0179 = 179;
    private static int testField0180 = 180;
    private static int testField0181 = 181;
    private static int testField0182 = 182;
    private static int testField0183 = 183;
    private static int testField0184 = 184;
    private static int testField0185 = 185;
    private static int testField0186 = 186;
    private static int testField0187 = 187;
    private static int testField0188 = 188;
    private static int testField0189 = 189;
    private static int testField0190 = 190;
    private static int testField0191 = 191;
    private static int testField0192 = 192;
    private static int testField0193 = 193;
    private static int testField0194 = 194;
    private static int testField0195 = 195;
    private static int testField0196 = 196;
    private static int testField0197 = 197;
    private static int testField0198 = 198;
    private static int testField0199 = 199;
    private static int testField0200 = 200;
    private static int testField0201 = 201;
    private static int testField0202 = 202;
    private static int testField0203 = 203;
    private static int testField0204 = 204;
    private static int testField0205 = 205;
    private static int testField0206 = 206;
    private static int testField0207 = 207;
    private static int testField0208 = 208;
    private static int testField0209 = 209;
    private static int testField0210 = 210;
    private static int testField0211 = 211;
    private static int testField0212 = 212;
    private static int testField0213 = 213;
    private static int testField0214 = 214;
    private static int testField0215 = 215;
    private static int testField0216 = 216;
    private static int testField0217 = 217;
    private static int testField0218 = 218;
    private static int testField0219 = 219;
    private static int testField0220 = 220;
    private static int testField0221 = 221;
    private static int testField0222 = 222;
    private static int testField0223 = 223;
    private static int testField0224 = 224;
    private static int testField0225 = 225;
    private static int testField0226 = 226;
    private static int testField0227 = 227;
    private static int testField0228 = 228;
    private static int testField0229 = 229;
    private static int testField0230 = 230;
    private static int testField0231 = 231;
    private static int testField0232 = 232;
    private static int testField0233 = 233;
    private static int testField0234 = 234;
    private static int testField0235 = 235;
    private static int testField0236 = 236;
    private static int testField0237 = 237;
    private static int testField0238 = 238;
    private static int testField0239 = 239;
    private static int testField0240 = 240;
    private static int testField0241 = 241;
    private static int testField0242 = 242;
    private static int testField0243 = 243;
    private static int testField0244 = 244;
    private static int testField0245 = 245;
    private static int testField0246 = 246;
    private static int testField0247 = 247;
    private static int testField0248 = 248;
    private static int testField0249 = 249;
    private static int testField0250 = 250;
    private static int testField0251 = 251;
    private static int testField0252 = 252;
    private static int testField0253 = 253;
    private static int testField0254 = 254;
    private static int testField0255 = 255;
    private static int testField0256 = 256;
    private static int testField0257 = 257;
    private static int testField0258 = 258;
    private static int testField0259 = 259;
    private static int testField0260 = 260;
    private static int testField0261 = 261;
    private static int testField0262 = 262;
    private static int testField0263 = 263;
    private static int testField0264 = 264;
    private static int testField0265 = 265;
    private static int testField0266 = 266;
    private static int testField0267 = 267;
    private static int testField0268 = 268;
    private static int testField0269 = 269;
    private static int testField0270 = 270;
    private static int testField0271 = 271;
    private static int testField0272 = 272;
    private static int testField0273 = 273;
    private static int testField0274 = 274;
    private static int testField0275 = 275;
    private static int testField0276 = 276;
    private static int testField0277 = 277;
    private static int testField0278 = 278;
    private static int testField0279 = 279;
    private static int testField0280 = 280;
    private static int testField0281 = 281;
    private static int testField0282 = 282;
    private static int testField0283 = 283;
    private static int testField0284 = 284;
    private static int testField0285 = 285;
    private static int testField0286 = 286;
    private static int testField0287 = 287;
    private static int testField0288 = 288;
    private static int testField0289 = 289;
    private static int testField0290 = 290;
    private static int testField0291 = 291;
    private static int testField0292 = 292;
    private static int testField0293 = 293;
    private static int testField0294 = 294;
    private static int testField0295 = 295;
    private static int testField0296 = 296;
    private static int testField0297 = 297;
    private static int testField0298 = 298;
    private static int testField0299 = 299;
    private static int testField0300 = 300;
    private static int testField0301 = 301;
    private static int testField0302 = 302;
    private static int testField0303 = 303;
    private static int testField0304 = 304;
    private static int testField0305 = 305;
    private static int testField0306 = 306;
    private static int testField0307 = 307;
    private static int testField0308 = 308;
    private static int testField0309 = 309;
    private static int testField0310 = 310;
    private static int testField0311 = 311;
    private static int testField0312 = 312;
    private static int testField0313 = 313;
    private static int testField0314 = 314;
    private static int testField0315 = 315;
    private static int testField0316 = 316;
    private static int testField0317 = 317;
    private static int testField0318 = 318;
    private static int testField0319 = 319;
    private static int testField0320 = 320;
    private static int testField0321 = 321;
    private static int testField0322 = 322;
    private static int testField0323 = 323;
    private static int testField0324 = 324;
    private static int testField0325 = 325;
    private static int testField0326 = 326;
    private static int testField0327 = 327;
    private static int testField0328 = 328;
    private static int testField0329 = 329;
    private static int testField0330 = 330;
    private static int testField0331 = 331;
    private static int testField0332 = 332;
    private static int testField0333 = 333;
    private static int testField0334 = 334;
    private static int testField0335 = 335;
    private static int testField0336 = 336;
    private static int testField0337 = 337;
    private static int testField0338 = 338;
    private static int testField0339 = 339;
    private static int testField0340 = 340;
    private static int testField0341 = 341;
    private static int testField0342 = 342;
    private static int testField0343 = 343;
    private static int testField0344 = 344;
    private static int testField0345 = 345;
    private static int testField0346 = 346;
    private static int testField0347 = 347;
    private static int testField0348 = 348;
    private static int testField0349 = 349;
    private static int testField0350 = 350;
    private static int testField0351 = 351;
    private static int testField0352 = 352;
    private static int testField0353 = 353;
    private static int testField0354 = 354;
    private static int testField0355 = 355;
    private static int testField0356 = 356;
    private static int testField0357 = 357;
    private static int testField0358 = 358;
    private static int testField0359 = 359;
    private static int testField0360 = 360;
    private static int testField0361 = 361;
    private static int testField0362 = 362;
    private static int testField0363 = 363;
    private static int testField0364 = 364;
    private static int testField0365 = 365;
    private static int testField0366 = 366;
    private static int testField0367 = 367;
    private static int testField0368 = 368;
    private static int testField0369 = 369;
    private static int testField0370 = 370;
    private static int testField0371 = 371;
    private static int testField0372 = 372;
    private static int testField0373 = 373;
    private static int testField0374 = 374;
    private static int testField0375 = 375;
    private static int testField0376 = 376;
    private static int testField0377 = 377;
    private static int testField0378 = 378;
    private static int testField0379 = 379;
    private static int testField0380 = 380;
    private static int testField0381 = 381;
    private static int testField0382 = 382;
    private static int testField0383 = 383;
    private static int testField0384 = 384;
    private static int testField0385 = 385;
    private static int testField0386 = 386;
    private static int testField0387 = 387;
    private static int testField0388 = 388;
    private static int testField0389 = 389;
    private static int testField0390 = 390;
    private static int testField0391 = 391;
    private static int testField0392 = 392;
    private static int testField0393 = 393;
    private static int testField0394 = 394;
    private static int testField0395 = 395;
    private static int testField0396 = 396;
    private static int testField0397 = 397;
    private static int testField0398 = 398;
    private static int testField0399 = 399;
    private static int testField0400 = 400;
    private static int testField0401 = 401;
    private static int testField0402 = 402;
    private static int testField0403 = 403;
    private static int testField0404 = 404;
    private static int testField0405 = 405;
    private static int testField0406 = 406;
    private static int testField0407 = 407;
    private static int testField0408 = 408;
    private static int testField0409 = 409;
    private static int testField0410 = 410;
    private static int testField0411 = 411;
    private static int testField0412 = 412;
    private static int testField0413 = 413;
    private static int testField0414 = 414;
    private static int testField0415 = 415;
    private static int testField0416 = 416;
    private static int testField0417 = 417;
    private static int testField0418 = 418;
    private static int testField0419 = 419;
    private static int testField0420 = 420;
    private static int testField0421 = 421;
    private static int testField0422 = 422;
    private static int testField0423 = 423;
    private static int testField0424 = 424;
    private static int testField0425 = 425;
    private static int testField0426 = 426;
    private static int testField0427 = 427;
    private static int testField0428 = 428;
    private static int testField0429 = 429;
    private static int testField0430 = 430;
    private static int testField0431 = 431;
    private static int testField0432 = 432;
    private static int testField0433 = 433;
    private static int testField0434 = 434;
    private static int testField0435 = 435;
    private static int testField0436 = 436;
    private static int testField0437 = 437;
    private static int testField0438 = 438;
    private static int testField0439 = 439;
    private static int testField0440 = 440;
    private static int testField0441 = 441;
    private static int testField0442 = 442;
    private static int testField0443 = 443;
    private static int testField0444 = 444;
    private static int testField0445 = 445;
    private static int testField0446 = 446;
    private static int testField0447 = 447;
    private static int testField0448 = 448;
    private static int testField0449 = 449;
    private static int testField0450 = 450;
    private static int testField0451 = 451;
    private static int testField0452 = 452;
    private static int testField0453 = 453;
    private static int testField0454 = 454;
    private static int testField0455 = 455;
    private static int testField0456 = 456;
    private static int testField0457 = 457;
    private static int testField0458 = 458;
    private static int testField0459 = 459;
    private static int testField0460 = 460;
    private static int testField0461 = 461;
    private static int testField0462 = 462;
    private static int testField0463 = 463;
    private static int testField0464 = 464;
    private static int testField0465 = 465;
    private static int testField0466 = 466;
    private static int testField0467 = 467;
    private static int testField0468 = 468;
    private static int testField0469 = 469;
    private static int testField0470 = 470;
    private static int testField0471 = 471;
    private static int testField0472 = 472;
    private static int testField0473 = 473;
    private static int testField0474 = 474;
    private static int testField0475 = 475;
    private static int testField0476 = 476;
    private static int testField0477 = 477;
    private static int testField0478 = 478;
    private static int testField0479 = 479;
    private static int testField0480 = 480;
    private static int testField0481 = 481;
    private static int testField0482 = 482;
    private static int testField0483 = 483;
    private static int testField0484 = 484;
    private static int testField0485 = 485;
    private static int testField0486 = 486;
    private static int testField0487 = 487;
    private static int testField0488 = 488;
    private static int testField0489 = 489;
    private static int testField0490 = 490;
    private static int testField0491 = 491;
    private static int testField0492 = 492;
    private static int testField0493 = 493;
    private static int testField0494 = 494;
    private static int testField0495 = 495;
    private static int testField0496 = 496;
    private static int testField0497 = 497;
    private static int testField0498 = 498;
    private static int testField0499 = 499;
    private static int testField0500 = 500;
    private static int testField0501 = 501;
    private static int testField0502 = 502;
    private static int testField0503 = 503;
    private static int testField0504 = 504;
    private static int testField0505 = 505;
    private static int testField0506 = 506;
    private static int testField0507 = 507;
    private static int testField0508 = 508;
    private static int testField0509 = 509;
    private static int testField0510 = 510;
    private static int testField0511 = 511;
    private static int testField0512 = 512;
    private static int testField0513 = 513;
    private static int testField0514 = 514;
    private static int testField0515 = 515;
    private static int testField0516 = 516;
    private static int testField0517 = 517;
    private static int testField0518 = 518;
    private static int testField0519 = 519;
    private static int testField0520 = 520;
    private static int testField0521 = 521;
    private static int testField0522 = 522;
    private static int testField0523 = 523;
    private static int testField0524 = 524;
    private static int testField0525 = 525;
    private static int testField0526 = 526;
    private static int testField0527 = 527;
    private static int testField0528 = 528;
    private static int testField0529 = 529;
    private static int testField0530 = 530;
    private static int testField0531 = 531;
    private static int testField0532 = 532;
    private static int testField0533 = 533;
    private static int testField0534 = 534;
    private static int testField0535 = 535;
    private static int testField0536 = 536;
    private static int testField0537 = 537;
    private static int testField0538 = 538;
    private static int testField0539 = 539;
    private static int testField0540 = 540;
    private static int testField0541 = 541;
    private static int testField0542 = 542;
    private static int testField0543 = 543;
    private static int testField0544 = 544;
    private static int testField0545 = 545;
    private static int testField0546 = 546;
    private static int testField0547 = 547;
    private static int testField0548 = 548;
    private static int testField0549 = 549;
    private static int testField0550 = 550;
    private static int testField0551 = 551;
    private static int testField0552 = 552;
    private static int testField0553 = 553;
    private static int testField0554 = 554;
    private static int testField0555 = 555;
    private static int testField0556 = 556;
    private static int testField0557 = 557;
    private static int testField0558 = 558;
    private static int testField0559 = 559;
    private static int testField0560 = 560;
    private static int testField0561 = 561;
    private static int testField0562 = 562;
    private static int testField0563 = 563;
    private static int testField0564 = 564;
    private static int testField0565 = 565;
    private static int testField0566 = 566;
    private static int testField0567 = 567;
    private static int testField0568 = 568;
    private static int testField0569 = 569;
    private static int testField0570 = 570;
    private static int testField0571 = 571;
    private static int testField0572 = 572;
    private static int testField0573 = 573;
    private static int testField0574 = 574;
    private static int testField0575 = 575;
    private static int testField0576 = 576;
    private static int testField0577 = 577;
    private static int testField0578 = 578;
    private static int testField0579 = 579;
    private static int testField0580 = 580;
    private static int testField0581 = 581;
    private static int testField0582 = 582;
    private static int testField0583 = 583;
    private static int testField0584 = 584;
    private static int testField0585 = 585;
    private static int testField0586 = 586;
    private static int testField0587 = 587;
    private static int testField0588 = 588;
    private static int testField0589 = 589;
    private static int testField0590 = 590;
    private static int testField0591 = 591;
    private static int testField0592 = 592;
    private static int testField0593 = 593;
    private static int testField0594 = 594;
    private static int testField0595 = 595;
    private static int testField0596 = 596;
    private static int testField0597 = 597;
    private static int testField0598 = 598;
    private static int testField0599 = 599;
    private static int testField0600 = 600;
    private static int testField0601 = 601;
    private static int testField0602 = 602;
    private static int testField0603 = 603;
    private static int testField0604 = 604;
    private static int testField0605 = 605;
    private static int testField0606 = 606;
    private static int testField0607 = 607;
    private static int testField0608 = 608;
    private static int testField0609 = 609;
    private static int testField0610 = 610;
    private static int testField0611 = 611;
    private static int testField0612 = 612;
    private static int testField0613 = 613;
    private static int testField0614 = 614;
    private static int testField0615 = 615;
    private static int testField0616 = 616;
    private static int testField0617 = 617;
    private static int testField0618 = 618;
    private static int testField0619 = 619;
    private static int testField0620 = 620;
    private static int testField0621 = 621;
    private static int testField0622 = 622;
    private static int testField0623 = 623;
    private static int testField0624 = 624;
    private static int testField0625 = 625;
    private static int testField0626 = 626;
    private static int testField0627 = 627;
    private static int testField0628 = 628;
    private static int testField0629 = 629;
    private static int testField0630 = 630;
    private static int testField0631 = 631;
    private static int testField0632 = 632;
    private static int testField0633 = 633;
    private static int testField0634 = 634;
    private static int testField0635 = 635;
    private static int testField0636 = 636;
    private static int testField0637 = 637;
    private static int testField0638 = 638;
    private static int testField0639 = 639;
    private static int testField0640 = 640;
    private static int testField0641 = 641;
    private static int testField0642 = 642;
    private static int testField0643 = 643;
    private static int testField0644 = 644;
    private static int testField0645 = 645;
    private static int testField0646 = 646;
    private static int testField0647 = 647;
    private static int testField0648 = 648;
    private static int testField0649 = 649;
    private static int testField0650 = 650;
    private static int testField0651 = 651;
    private static int testField0652 = 652;
    private static int testField0653 = 653;
    private static int testField0654 = 654;
    private static int testField0655 = 655;
    private static int testField0656 = 656;
    private static int testField0657 = 657;
    private static int testField0658 = 658;
    private static int testField0659 = 659;
    private static int testField0660 = 660;
    private static int testField0661 = 661;
    private static int testField0662 = 662;
    private static int testField0663 = 663;
    private static int testField0664 = 664;
    private static int testField0665 = 665;
    private static int testField0666 = 666;
    private static int testField0667 = 667;
    private static int testField0668 = 668;
    private static int testField0669 = 669;
    private static int testField0670 = 670;
    private static int testField0671 = 671;
    private static int testField0672 = 672;
    private static int testField0673 = 673;
    private static int testField0674 = 674;
    private static int testField0675 = 675;
    private static int testField0676 = 676;
    private static int testField0677 = 677;
    private static int testField0678 = 678;
    private static int testField0679 = 679;
    private static int testField0680 = 680;
    private static int testField0681 = 681;
    private static int testField0682 = 682;
    private static int testField0683 = 683;
    private static int testField0684 = 684;
    private static int testField0685 = 685;
    private static int testField0686 = 686;
    private static int testField0687 = 687;
    private static int testField0688 = 688;
    private static int testField0689 = 689;
    private static int testField0690 = 690;
    private static int testField0691 = 691;
    private static int testField0692 = 692;
    private static int testField0693 = 693;
    private static int testField0694 = 694;
    private static int testField0695 = 695;
    private static int testField0696 = 696;
    private static int testField0697 = 697;
    private static int testField0698 = 698;
    private static int testField0699 = 699;
    private static int testField0700 = 700;
    private static int testField0701 = 701;
    private static int testField0702 = 702;
    private static int testField0703 = 703;
    private static int testField0704 = 704;
    private static int testField0705 = 705;
    private static int testField0706 = 706;
    private static int testField0707 = 707;
    private static int testField0708 = 708;
    private static int testField0709 = 709;
    private static int testField0710 = 710;
    private static int testField0711 = 711;
    private static int testField0712 = 712;
    private static int testField0713 = 713;
    private static int testField0714 = 714;
    private static int testField0715 = 715;
    private static int testField0716 = 716;
    private static int testField0717 = 717;
    private static int testField0718 = 718;
    private static int testField0719 = 719;
    private static int testField0720 = 720;
    private static int testField0721 = 721;
    private static int testField0722 = 722;
    private static int testField0723 = 723;
    private static int testField0724 = 724;
    private static int testField0725 = 725;
    private static int testField0726 = 726;
    private static int testField0727 = 727;
    private static int testField0728 = 728;
    private static int testField0729 = 729;
    private static int testField0730 = 730;
    private static int testField0731 = 731;
    private static int testField0732 = 732;
    private static int testField0733 = 733;
    private static int testField0734 = 734;
    private static int testField0735 = 735;
    private static int testField0736 = 736;
    private static int testField0737 = 737;
    private static int testField0738 = 738;
    private static int testField0739 = 739;
    private static int testField0740 = 740;
    private static int testField0741 = 741;
    private static int testField0742 = 742;
    private static int testField0743 = 743;
    private static int testField0744 = 744;
    private static int testField0745 = 745;
    private static int testField0746 = 746;
    private static int testField0747 = 747;
    private static int testField0748 = 748;
    private static int testField0749 = 749;
    private static int testField0750 = 750;
    private static int testField0751 = 751;
    private static int testField0752 = 752;
    private static int testField0753 = 753;
    private static int testField0754 = 754;
    private static int testField0755 = 755;
    private static int testField0756 = 756;
    private static int testField0757 = 757;
    private static int testField0758 = 758;
    private static int testField0759 = 759;
    private static int testField0760 = 760;
    private static int testField0761 = 761;
    private static int testField0762 = 762;
    private static int testField0763 = 763;
    private static int testField0764 = 764;
    private static int testField0765 = 765;
    private static int testField0766 = 766;
    private static int testField0767 = 767;
    private static int testField0768 = 768;
    private static int testField0769 = 769;
    private static int testField0770 = 770;
    private static int testField0771 = 771;
    private static int testField0772 = 772;
    private static int testField0773 = 773;
    private static int testField0774 = 774;
    private static int testField0775 = 775;
    private static int testField0776 = 776;
    private static int testField0777 = 777;
    private static int testField0778 = 778;
    private static int testField0779 = 779;
    private static int testField0780 = 780;
    private static int testField0781 = 781;
    private static int testField0782 = 782;
    private static int testField0783 = 783;
    private static int testField0784 = 784;
    private static int testField0785 = 785;
    private static int testField0786 = 786;
    private static int testField0787 = 787;
    private static int testField0788 = 788;
    private static int testField0789 = 789;
    private static int testField0790 = 790;
    private static int testField0791 = 791;
    private static int testField0792 = 792;
    private static int testField0793 = 793;
    private static int testField0794 = 794;
    private static int testField0795 = 795;
    private static int testField0796 = 796;
    private static int testField0797 = 797;
    private static int testField0798 = 798;
    private static int testField0799 = 799;
    private static int testField0800 = 800;
    private static int testField0801 = 801;
    private static int testField0802 = 802;
    private static int testField0803 = 803;
    private static int testField0804 = 804;
    private static int testField0805 = 805;
    private static int testField0806 = 806;
    private static int testField0807 = 807;
    private static int testField0808 = 808;
    private static int testField0809 = 809;
    private static int testField0810 = 810;
    private static int testField0811 = 811;
    private static int testField0812 = 812;
    private static int testField0813 = 813;
    private static int testField0814 = 814;
    private static int testField0815 = 815;
    private static int testField0816 = 816;
    private static int testField0817 = 817;
    private static int testField0818 = 818;
    private static int testField0819 = 819;
    private static int testField0820 = 820;
    private static int testField0821 = 821;
    private static int testField0822 = 822;
    private static int testField0823 = 823;
    private static int testField0824 = 824;
    private static int testField0825 = 825;
    private static int testField0826 = 826;
    private static int testField0827 = 827;
    private static int testField0828 = 828;
    private static int testField0829 = 829;
    private static int testField0830 = 830;
    private static int testField0831 = 831;
    private static int testField0832 = 832;
    private static int testField0833 = 833;
    private static int testField0834 = 834;
    private static int testField0835 = 835;
    private static int testField0836 = 836;
    private static int testField0837 = 837;
    private static int testField0838 = 838;
    private static int testField0839 = 839;
    private static int testField0840 = 840;
    private static int testField0841 = 841;
    private static int testField0842 = 842;
    private static int testField0843 = 843;
    private static int testField0844 = 844;
    private static int testField0845 = 845;
    private static int testField0846 = 846;
    private static int testField0847 = 847;
    private static int testField0848 = 848;
    private static int testField0849 = 849;
    private static int testField0850 = 850;
    private static int testField0851 = 851;
    private static int testField0852 = 852;
    private static int testField0853 = 853;
    private static int testField0854 = 854;
    private static int testField0855 = 855;
    private static int testField0856 = 856;
    private static int testField0857 = 857;
    private static int testField0858 = 858;
    private static int testField0859 = 859;
    private static int testField0860 = 860;
    private static int testField0861 = 861;
    private static int testField0862 = 862;
    private static int testField0863 = 863;
    private static int testField0864 = 864;
    private static int testField0865 = 865;
    private static int testField0866 = 866;
    private static int testField0867 = 867;
    private static int testField0868 = 868;
    private static int testField0869 = 869;
    private static int testField0870 = 870;
    private static int testField0871 = 871;
    private static int testField0872 = 872;
    private static int testField0873 = 873;
    private static int testField0874 = 874;
    private static int testField0875 = 875;
    private static int testField0876 = 876;
    private static int testField0877 = 877;
    private static int testField0878 = 878;
    private static int testField0879 = 879;
    private static int testField0880 = 880;
    private static int testField0881 = 881;
    private static int testField0882 = 882;
    private static int testField0883 = 883;
    private static int testField0884 = 884;
    private static int testField0885 = 885;
    private static int testField0886 = 886;
    private static int testField0887 = 887;
    private static int testField0888 = 888;
    private static int testField0889 = 889;
    private static int testField0890 = 890;
    private static int testField0891 = 891;
    private static int testField0892 = 892;
    private static int testField0893 = 893;
    private static int testField0894 = 894;
    private static int testField0895 = 895;
    private static int testField0896 = 896;
    private static int testField0897 = 897;
    private static int testField0898 = 898;
    private static int testField0899 = 899;
    private static int testField0900 = 900;
    private static int testField0901 = 901;
    private static int testField0902 = 902;
    private static int testField0903 = 903;
    private static int testField0904 = 904;
    private static int testField0905 = 905;
    private static int testField0906 = 906;
    private static int testField0907 = 907;
    private static int testField0908 = 908;
    private static int testField0909 = 909;
    private static int testField0910 = 910;
    private static int testField0911 = 911;
    private static int testField0912 = 912;
    private static int testField0913 = 913;
    private static int testField0914 = 914;
    private static int testField0915 = 915;
    private static int testField0916 = 916;
    private static int testField0917 = 917;
    private static int testField0918 = 918;
    private static int testField0919 = 919;
    private static int testField0920 = 920;
    private static int testField0921 = 921;
    private static int testField0922 = 922;
    private static int testField0923 = 923;
    private static int testField0924 = 924;
    private static int testField0925 = 925;
    private static int testField0926 = 926;
    private static int testField0927 = 927;
    private static int testField0928 = 928;
    private static int testField0929 = 929;
    private static int testField0930 = 930;
    private static int testField0931 = 931;
    private static int testField0932 = 932;
    private static int testField0933 = 933;
    private static int testField0934 = 934;
    private static int testField0935 = 935;
    private static int testField0936 = 936;
    private static int testField0937 = 937;
    private static int testField0938 = 938;
    private static int testField0939 = 939;
    private static int testField0940 = 940;
    private static int testField0941 = 941;
    private static int testField0942 = 942;
    private static int testField0943 = 943;
    private static int testField0944 = 944;
    private static int testField0945 = 945;
    private static int testField0946 = 946;
    private static int testField0947 = 947;
    private static int testField0948 = 948;
    private static int testField0949 = 949;
    private static int testField0950 = 950;
    private static int testField0951 = 951;
    private static int testField0952 = 952;
    private static int testField0953 = 953;
    private static int testField0954 = 954;
    private static int testField0955 = 955;
    private static int testField0956 = 956;
    private static int testField0957 = 957;
    private static int testField0958 = 958;
    private static int testField0959 = 959;
    private static int testField0960 = 960;
    private static int testField0961 = 961;
    private static int testField0962 = 962;
    private static int testField0963 = 963;
    private static int testField0964 = 964;
    private static int testField0965 = 965;
    private static int testField0966 = 966;
    private static int testField0967 = 967;
    private static int testField0968 = 968;
    private static int testField0969 = 969;
    private static int testField0970 = 970;
    private static int testField0971 = 971;
    private static int testField0972 = 972;
    private static int testField0973 = 973;
    private static int testField0974 = 974;
    private static int testField0975 = 975;
    private static int testField0976 = 976;
    private static int testField0977 = 977;
    private static int testField0978 = 978;
    private static int testField0979 = 979;
    private static int testField0980 = 980;
    private static int testField0981 = 981;
    private static int testField0982 = 982;
    private static int testField0983 = 983;
    private static int testField0984 = 984;
    private static int testField0985 = 985;
    private static int testField0986 = 986;
    private static int testField0987 = 987;
    private static int testField0988 = 988;
    private static int testField0989 = 989;
    private static int testField0990 = 990;
    private static int testField0991 = 991;
    private static int testField0992 = 992;
    private static int testField0993 = 993;
    private static int testField0994 = 994;
    private static int testField0995 = 995;
    private static int testField0996 = 996;
    private static int testField0997 = 997;
    private static int testField0998 = 998;
    private static int testField0999 = 999;
    private static int testField1000 = 1000;
    private static int testField1001 = 1001;
    private static int testField1002 = 1002;
    private static int testField1003 = 1003;
    private static int testField1004 = 1004;
    private static int testField1005 = 1005;
    private static int testField1006 = 1006;
    private static int testField1007 = 1007;
    private static int testField1008 = 1008;
    private static int testField1009 = 1009;
    private static int testField1010 = 1010;
    private static int testField1011 = 1011;
    private static int testField1012 = 1012;
    private static int testField1013 = 1013;
    private static int testField1014 = 1014;
    private static int testField1015 = 1015;
    private static int testField1016 = 1016;
    private static int testField1017 = 1017;
    private static int testField1018 = 1018;
    private static int testField1019 = 1019;
    private static int testField1020 = 1020;
    private static int testField1021 = 1021;
    private static int testField1022 = 1022;
    private static int testField1023 = 1023;
}
