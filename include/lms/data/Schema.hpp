#pragma once
namespace lms {
/// The SQL text of sql/schema.sql, embedded at build time.
const char* schemaSql();
inline constexpr int kSchemaVersion = 1;
}  // namespace lms
