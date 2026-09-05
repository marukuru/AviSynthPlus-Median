with open("Median/median.cpp", "r") as f:
    code = f.read()

code = code.replace("job->filter->template ProcessPixel_T<uint8_t>(values)", "job->filter->ProcessPixel(values)")

with open("Median/median.cpp", "w") as f:
    f.write(code)
