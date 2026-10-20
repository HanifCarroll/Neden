import QtQuick.Controls.Basic as Basic

// The enclosing layout owns the width. Child tabs divide that width without
// feeding their calculated sizes back into the bar's implicit size.
Basic.TabBar {
    implicitWidth: 0
}
