define dso_local i32 @foo(i32 noundef %0) #0 {
  ; a = b + 1
  %a = add i32 %0, 1

  ; c = a - 1
  %c = sub i32 %a, 1

  ; Restituisci il valore di c
  ret i32 %c
}