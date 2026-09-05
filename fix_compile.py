import re

with open("Median/median.h", "r") as f:
    code = f.read()
code = code.replace("struct MedianJobData {", "class Median;\nstruct MedianJobData {")
# change const Median* to Median* so we can call non-const or const freely. But the template issue needs `template ProcessPixel_T<T>`
with open("Median/median.h", "w") as f:
    f.write(code)

with open("Median/median.cpp", "r") as f:
    code = f.read()

code = code.replace("job->filter->ProcessPixel_T<uint8_t>(values)", "job->filter->template ProcessPixel_T<uint8_t>(values)")
code = code.replace("job->filter->ProcessPixel_T<uint16_t>(values)", "job->filter->template ProcessPixel_T<uint16_t>(values)")
code = code.replace("job->filter->ProcessPixel_T<float>(values)", "job->filter->template ProcessPixel_T<float>(values)")

code = code.replace("if (env->CheckVersion(6)) env2 = static_cast<IScriptEnvironment2*>(env);", "env->CheckVersion(6); env2 = static_cast<IScriptEnvironment2*>(env);")

with open("Median/median.cpp", "w") as f:
    f.write(code)
