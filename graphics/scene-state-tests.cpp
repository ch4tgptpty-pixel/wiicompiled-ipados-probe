#include "../aurora-main/tests/gx_test_common.hpp"
#include "scene.h"

TEST_F(GXFifoTest, ProbeScissorCoversRetinaSurfaceAfterRotation) {
  aurora::gfx::testing::use_real_vertex_format_helpers(true);
  const std::array<std::array<uint32_t, 2>, 4> sizes{{
      {640, 480}, {1964, 1620}, {2160, 1620}, {1620, 2160}}};
  for (const auto size : sizes) {
    SCOPED_TRACE(::testing::Message() << size[0] << "x" << size[1]);
    aurora::gfx::testing::set_framebuffer_sizes(size[0], size[1], size[0], size[1]);
    DrawProbeScene(0, size[0], size[1]);
    aurora::gx::fifo::drain();
    const auto& scissor = gxState().renderScissor;
    EXPECT_EQ(scissor.x, 0);
    EXPECT_EQ(scissor.y, 0);
    EXPECT_EQ(scissor.width, size[0]);
    EXPECT_EQ(scissor.height, size[1]);
    EXPECT_FLOAT_EQ(gxState().renderViewport.width, float(size[0]));
    EXPECT_FLOAT_EQ(gxState().renderViewport.height, float(size[1]));
    EXPECT_EQ(aurora::gfx::testing::last_pushed_indices().size(), 3);
  }
}
