; ModuleID = './test/Foo.c'
source_filename = "./test/Foo.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@a = dso_local local_unnamed_addr global i32 0, align 4
@d = dso_local local_unnamed_addr global i32 0, align 4
@e = dso_local local_unnamed_addr global i32 0, align 4
@b = dso_local local_unnamed_addr global i32 3, align 4
@c = dso_local local_unnamed_addr global i32 2, align 4

; Function Attrs: nofree norecurse nosync nounwind memory(readwrite, argmem: none, inaccessiblemem: none) uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
  %1 = load i32, ptr @a, align 4, !tbaa !5
  %2 = load i32, ptr @d, align 4, !tbaa !5
  %3 = load i32, ptr @b, align 4
  %4 = load i32, ptr @c, align 4
  %5 = add nsw i32 %4, %3
  br label %6

6:                                                ; preds = %0, %13
  %7 = phi i32 [ %2, %0 ], [ %15, %13 ]
  %8 = phi i32 [ %1, %0 ], [ %14, %13 ]
  %9 = icmp eq i32 %8, 2
  br i1 %9, label %10, label %11

10:                                               ; preds = %6
  store i32 %5, ptr @a, align 4, !tbaa !5
  br label %13

11:                                               ; preds = %6
  %12 = icmp eq i32 %7, 6
  br i1 %12, label %16, label %13

13:                                               ; preds = %11, %10
  %14 = phi i32 [ %8, %11 ], [ %5, %10 ]
  %15 = add nsw i32 %14, 1
  store i32 %15, ptr @d, align 4, !tbaa !5
  br label %6

16:                                               ; preds = %11
  store i32 3, ptr @e, align 4, !tbaa !5
  ret i32 0
}

attributes #0 = { nofree norecurse nosync nounwind memory(readwrite, argmem: none, inaccessiblemem: none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"Ubuntu clang version 19.1.7 (++20250114103320+cd708029e0b2-1~exp1~20250114103432.75)"}
!5 = !{!6, !6, i64 0}
!6 = !{!"int", !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C/C++ TBAA"}
