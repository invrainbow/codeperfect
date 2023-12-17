import { A } from "@/components/A";
import { Icon } from "@/components/Icon";
import { links } from "@/links";
import { Box } from "./components/Box";
import { Span } from "./components/Span";
import { Flex } from "./components/Flex";
import { Button } from "./components/Button";

export const Hero = () => (
  <Flex cx="flex-col gap-6 md:gap-8 max-w-full leading-relaxed px-4">
    <Box cx="md:text-center font-bold text-5xl text-black tracking-tight leading-[1.1] md:leading-none">
      A fast, lightweight Go IDE
    </Box>
    <Flex cx="md:text-center flex-col items-stretch self-stretch md:self-auto md:items-center md:flex-row gap-2 md:gap-4 md:justify-center">
      <Button href={links.download}>
        <Icon size={18} icon="Download" />
        Download for Mac
      </Button>
      <Button variant="secondary" href={links.docs}>
        View Docs
        <Icon
          size={18}
          icon="ChevronRight"
          cx="relative group-hover:translate-x-1 transition"
        />
      </Button>
    </Flex>
    <Box cx="md:text-center">
      <Span cx="text-slate-500 text-sm font-medium leading-normal md:leading-none inline-block">
        <b>Note:</b> CodePerfect is no longer actively developed, but is now{" "}
        <A href={links.github}>open source</A> and free.
      </Span>
    </Box>
  </Flex>
);
