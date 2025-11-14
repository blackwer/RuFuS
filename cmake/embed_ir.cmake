file(READ ${IR_FILE} IR_CONTENT)
file(WRITE ${HEADER_FILE}
"#pragma once
namespace rufus::embedded {
constexpr const char* ${VAR_NAME} = R\"IR_DELIM(
${IR_CONTENT}
)IR_DELIM\";
}
")
