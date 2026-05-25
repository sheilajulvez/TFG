with open("DescripcionTrabajoConcursosProgramación.tex", "r", encoding="utf8") as f:
    text = f.read()

fixed = text.encode("cp1252").decode("utf8")

with open("DescripcionTrabajoConcursosProgramación_FIXED.tex", "w", encoding="utf8") as f:
    f.write(fixed)

print("Arreglado")