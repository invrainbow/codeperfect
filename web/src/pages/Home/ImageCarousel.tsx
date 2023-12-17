import cx from "classnames";
import { useEffect, useState } from "react";

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
    <div className="flex flex-col-reverse md:flex-row gap-4 md:gap-6 items-center">
      <div className="flex flex-row md:flex-col gap-3">
        {IMAGES.map((it, idx) => (
          <div
            className={cx(
              "h-3 w-3 md:w-4 md:h-4 rounded-full cursor-pointer transition-colors",
              current === idx
                ? "bg-neutral-400"
                : "bg-neutral-400/30 hover:bg-neutral-400/50"
            )}
            onClick={() => setCurrent(idx)}
            key={it}
          />
        ))}
      </div>
      <div className="relative flex-1">
        <img
          className="opacity-0 max-w-full"
          alt="screenshot"
          src={IMAGES[0]}
        />
        {IMAGES.map((it, idx) => (
          <div className="absolute inset-0 flex items-center justify-start">
            <img
              className={cx(
                "max-w-full border border-neutral-400 shadow-lg rounded-xl overflow-hidden",
                idx === current ? "opacity-100" : "opacity-0"
              )}
              alt="screenshot"
              src={it}
            />
          </div>
        ))}
      </div>
    </div>
  );
};
