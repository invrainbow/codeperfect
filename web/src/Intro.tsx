import { links } from "@/links";
import { Button } from "./components/Button";
import dom from "./components/dom";
import { Icon } from "./components/Icon";

const BAD_FEATURES = [
  "Electron",
  "JavaScript",
  "language servers",
  "frameworks",
];

export const Intro = () => (
  <dom.div cx="text-white py-20 md:py-28 px-4 relative  overflow-x-clip">
    <dom.div cx="bg-neutral-900 absolute inset-0 -rotate-[1.5deg] scale-x-150" />
    <dom.div cx="flex flex-col items-center md:w-[400px] md:mx-auto gap-6 relative z-20">
      <dom.div cx="flex flex-col items-center leading-none gap-1 text-[160%] md:text-4xl font-extrabold text-white">
        {BAD_FEATURES.map((name) => (
          <dom.div key={name} cx="md:whitespace-nowrap leading-none">
            No {name}.
          </dom.div>
        ))}
      </dom.div>
      <dom.div cx="leading-normal text-center text-neutral-400">
        CodePerfect is built from scratch in C/C++/OpenGL like a video game.
        Instant startup, 144 FPS, near-zero latency, native code intelligence.
      </dom.div>
      <dom.div>
        <Button variant="inverse" href={links.docs}>
          See features
          <Icon
            size={18}
            cx="relative group-hover:translate-x-1 transition"
            icon="ChevronRight"
          />
        </Button>
      </dom.div>
    </dom.div>
  </dom.div>
);
