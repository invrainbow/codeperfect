import { links } from "@/links";
import { ImageCarousel } from "./ImageCarousel";
import { Box } from "./components/Box";
import { Button } from "./components/Button";
import { Flex } from "./components/Flex";

const BAD_FEATURES = [
  "Electron",
  "JavaScript",
  "language servers",
  "frameworks",
];

export const Intro = () => (
  <Flex cx="px-4 md:px-8 md:py-4 max-w-screen-xl mx-auto flex-col md:flex-row gap-16">
    <Flex cx="flex-col items-start md:w-1/3 md:mx-0 gap-6">
      <Flex cx="flex-col items-start leading-none gap-2 text-[160%] md:text-[200%] font-extrabold text-black">
        {BAD_FEATURES.map((name) => (
          <Box key={name} cx="text-black md:whitespace-nowrap leading-none">
            No {name}.
          </Box>
        ))}
      </Flex>
      <Box cx="leading-relaxed text-neutral-600">
        CodePerfect is built from scratch in C/C++/OpenGL like a video game.
        Instant startup, 144 FPS, near-zero latency, native code intelligence.
      </Box>
      <Box>
        <Button href={links.docs}>See features</Button>
      </Box>
    </Flex>
    <Box cx="lg:flex-1">
      <ImageCarousel />
    </Box>
  </Flex>
);
