import { useEffect, useState } from "react";
import dom from "./components/dom";

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
    <dom.div cx="flex flex-col-reverse md:items-center md:flex-row gap-4 md:gap-6">
      <dom.div cx="flex flex-row md:flex-col gap-3">
        {IMAGES.map((it, idx) => (
          <dom.div
            cx={[
              "h-3 w-3 md:w-4 md:h-4 rounded-full cursor-pointer transition-colors",
              current === idx && "bg-neutral-400",
              current !== idx && "bg-neutral-400/30 hover:bg-neutral-400/50",
            ]}
            onClick={() => setCurrent(idx)}
            key={it}
          />
        ))}
      </dom.div>
      <dom.div cx="relative flex-1">
        <dom.img cx="opacity-0 max-w-full" src={IMAGES[0]} />
        {IMAGES.map((it, idx) => (
          <dom.div cx="flex absolute inset-0 justify-start">
            <dom.img
              cx={[
                "max-w-full border border-neutral-400 shadow-lg rounded-xl overflow-hidden",
                idx === current ? "opacity-100" : "opacity-0",
              ]}
              src={it}
            />
          </dom.div>
        ))}
      </dom.div>
    </dom.div>
  );
};
