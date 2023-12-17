import { A } from "./A";
import { Flex, Props as FlexProps } from "./Flex";

interface Props extends FlexProps<"a"> {
  variant?: "primary" | "secondary";
  block?: boolean;
  disabled?: boolean;
}

export const Button = ({
  cx,
  href,
  variant = "primary",
  block,
  disabled,
  ...rest
}: Props) => (
  <Flex
    as={A}
    href={href}
    cx={[
      "justify-center text-center group gap-1",
      "rounded-md leading-none py-4 px-6",
      "no-underline outline-none whitespace-nowrap",
      "text-base font-sans font-medium box-border",
      disabled && "disabled:opacity-30 disabled:cursor-not-allowed",
      !disabled &&
        "hover:scale-[1.015] active:top-[1px] active:scale-[.985] transition-transform duration-100",
      variant === "primary" && "bg-primary text-white saturate-[0.8]",
      variant === "secondary" && "bg-neutral-200 text-neutral-600",
      block ? "flex" : "inline-flex",
      cx,
    ]}
    {...rest}
  />
);
