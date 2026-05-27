/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Author:    Ajeet Singh Yadav
 * Created:   May 2026
 *
 * Autodoc:   yes
 * ----------------------------------------------------------------------
 */

#include <gtest/gtest.h>

#include "vertexnova/sc/vnesc.h"

namespace vne::sc {

class ResultCodeTest : public ::testing::Test {};

TEST_F(ResultCodeTest, SucceededForNonNegative) {
    EXPECT_TRUE(succeeded(ResultCode::eSuccess));
    EXPECT_TRUE(succeeded(ResultCode::eCompileWarnings));
}

TEST_F(ResultCodeTest, FailedForNegative) {
    EXPECT_FALSE(succeeded(ResultCode::eCompileFailed));
    EXPECT_FALSE(succeeded(ResultCode::eCrossCompileFailed));
    EXPECT_FALSE(succeeded(ResultCode::eUnavailable));
    EXPECT_FALSE(succeeded(ResultCode::eFileNotFound));
}

TEST_F(ResultCodeTest, OkHelperMatchesSucceeded) {
    CompileResult cr;
    cr.code = ResultCode::eSuccess;
    EXPECT_TRUE(cr.ok());
    cr.code = ResultCode::eCompileFailed;
    EXPECT_FALSE(cr.ok());
}

}  // namespace vne::sc
