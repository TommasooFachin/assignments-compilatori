define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) #0 {
  ; Addizione con 0: %3 = add nsw i32 %1, 0
  ; Questa sarà ottimizzata in %3 = %1
  %3 = add nsw i32 %1, 0

  ; Moltiplicazione per 1: 
  %4 = mul nsw i32 %1, 1
  ; Questa sarà ottimizzata in %4 = %1

  ret i32 %3
}