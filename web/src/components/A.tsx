import { Box, Props as BoxProps } from "./Box";

export interface Props extends BoxProps<"a"> {
  newWindow?: boolean;
}

export const A = ({ cx, newWindow = true, ...rest }: Props) => (
  <Box
    as="a"
    target={newWindow ? "_blank" : undefined}
    cx={["text-primary underline break-words underline-offset-2", cx]}
    {...rest}
  />
);
