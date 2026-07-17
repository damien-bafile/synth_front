#include "unity.h"
#include "unity_fixture.h"

TEST_GROUP(Protocol);
TEST_GROUP(Framebuffer);
TEST_GROUP(Input);

TEST_GROUP_RUNNER(Protocol)
{
  RUN_TEST_CASE(Protocol, EncodeDecodeRoundTrip);
  RUN_TEST_CASE(Protocol, RejectsBadSync);
  RUN_TEST_CASE(Protocol, RejectsBadChecksum);
  RUN_TEST_CASE(Protocol, ReturnsZeroOnIncomplete);
  RUN_TEST_CASE(Protocol, EmptyPayload);
  RUN_TEST_CASE(Protocol, LargePayload);
  RUN_TEST_CASE(Protocol, AllPacketTypesRoundTrip);
}

TEST_GROUP_RUNNER(Framebuffer)
{
  RUN_TEST_CASE(Framebuffer, InitAndGetFrame);
  RUN_TEST_CASE(Framebuffer, NoFrameAvailable);
  RUN_TEST_CASE(Framebuffer, RejectsOversized);
  RUN_TEST_CASE(Framebuffer, DoubleBuffering);
  RUN_TEST_CASE(Framebuffer, MultipleFrames);
}

TEST_GROUP_RUNNER(Input)
{
  RUN_TEST_CASE(Input, ArrowKeys);
  RUN_TEST_CASE(Input, ReturnAndSpace);
  RUN_TEST_CASE(Input, FunctionKeysF1F8);
  RUN_TEST_CASE(Input, FunctionKeysF9F12);
  RUN_TEST_CASE(Input, NumberKeys);
  RUN_TEST_CASE(Input, NoteKeys);
  RUN_TEST_CASE(Input, EncoderKeysNoShift);
  RUN_TEST_CASE(Input, EncoderKeysWithShift);
  RUN_TEST_CASE(Input, ExtraButtons);
  RUN_TEST_CASE(Input, TransportControls);
  RUN_TEST_CASE(Input, CommaKey);
  RUN_TEST_CASE(Input, PeriodKey);
  RUN_TEST_CASE(Input, UnknownKey);
}

static void RunAllTests(void)
{
  RUN_TEST_GROUP(Protocol);
  RUN_TEST_GROUP(Framebuffer);
  RUN_TEST_GROUP(Input);
}

int main(int argc, char* argv[])
{
  return UnityMain(argc, (const char**)argv, RunAllTests);
}
