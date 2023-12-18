import { links } from "@/links";
import { ImageCarousel } from "./ImageCarousel";
import dom from "./components/dom";
import { Button } from "./components/Button";

const BAD_FEATURES = [
  "Electron",
  "JavaScript",
  "language servers",
  "frameworks",
];

export const Intro = () => (
  <dom.div cx="flex items-center px-4 md:px-8 md:py-4 max-w-screen-xl mx-auto flex-col md:flex-row gap-16">
    <dom.div cx="flex flex-col items-start md:w-1/3 md:mx-0 gap-6">
      <dom.div cx="flex flex-col items-start leading-none gap-2 text-[160%] md:text-[200%] font-extrabold text-black">
        {BAD_FEATURES.map((name) => (
          <dom.div key={name} cx="text-black md:whitespace-nowrap leading-none">
            No {name}.
          </dom.div>
        ))}
      </dom.div>
      <dom.div cx="leading-relaxed text-neutral-600">
        CodePerfect is built from scratch in C/C++/OpenGL like a video game.
        Instant startup, 144 FPS, near-zero latency, native code intelligence.
      </dom.div>
      <dom.div>
        <Button href={links.docs}>See features</Button>
      </dom.div>
    </dom.div>
    <dom.div cx="lg:flex-1">
      <ImageCarousel />
    </dom.div>
  </dom.div>
);
