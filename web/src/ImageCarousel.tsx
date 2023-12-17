import { useEffect, useState } from "react";
import { Box } from "./components/Box";
import { Flex } from "./components/Flex";
import { Image } from "./components/Image";

const IMAGES = [
  "/feature-autocomplete.png",
  "/feature-command-palette.png",
  "/feature-find-references.png",
  "/feature-generate-implementation.png",
  "/feature-goto-file.png",
  "/feature-postfix.png",
];

export const ImageCarousel = () => {
  const [current, setCurrent] = useState(0);

  useEffect(() => {
    const timeout = setTimeout(() => {
      setCurrent((current + 1) % IMAGES.length);
    }, 5000);
    return () => clearTimeout(timeout);
  }, [current]);

  return (
    <Flex cx="flex-col-reverse md:flex-row gap-4 md:gap-6">
      <Flex cx="flex-row md:flex-col gap-3">
        {IMAGES.map((it, idx) => (
          <Box
            cx={[
              "h-3 w-3 md:w-4 md:h-4 rounded-full cursor-pointer transition-colors",
              current === idx && "bg-neutral-400",
              current !== idx && "bg-neutral-400/30 hover:bg-neutral-400/50",
            ]}
            onClick={() => setCurrent(idx)}
            key={it}
          />
        ))}
      </Flex>
      <Box cx="relative flex-1">
        <Image cx="opacity-0 max-w-full" src={IMAGES[0]} />
        {IMAGES.map((it, idx) => (
          <Flex cx="absolute inset-0 justify-start">
            <Image
              cx={[
                "max-w-full border border-neutral-400 shadow-lg rounded-xl overflow-hidden",
                idx === current ? "opacity-100" : "opacity-0",
              ]}
              src={it}
            />
          </Flex>
        ))}
      </Box>
    </Flex>
  );
};
